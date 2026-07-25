#include "vaudio.h"
#include <cstring>
#include <cmath>
#include <algorithm>

// --- SATURATOR DSP PARAMETERS ---
float g_drive = 0.5f;
int   g_mode = 0;

// --- COMPRESSOR DSP PARAMETERS ---
float g_comp_thresh_db = -12.0f; // -60 dB to 0 dB
float g_comp_ratio     = 4.0f;   // 1.0 to 20.0
float g_comp_attack_ms = 15.0f;  // 0.1 ms to 100 ms
float g_comp_release_ms= 120.0f; // 10 ms to 1000 ms
float g_comp_makeup_db = 3.0f;   // 0 dB to 24 dB
bool  g_comp_enabled   = true;

// --- REVERB DSP PARAMETERS ---
float g_rev_decay = 0.5f;   // Decay time / Room Size (0.0 to 0.95)
float g_rev_mix   = 0.3f;   // Wet/Dry mix (0.0 = Dry, 1.0 = Wet)
bool  g_rev_enabled = true;

static float g_envelope = 0.0f; // Envelope detector state across blocks
static float g_out_envelope = 0.0f;

// --- SATURATOR CALLBACK ---
void SaturationProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    float gain = 1.0f + (g_drive * 7.0f);
    float makeup = 1.0f / std::sqrt(gain);

    for (unsigned int i = 0; i < frames * 2; i++) {
        float sample = samples[i] * gain;

        switch (g_mode) {
            case 0: // SOFT TUBE
                sample = std::tanh(sample);
                break;
            case 1: // HARD CLIP
                sample = std::clamp(sample, -0.7f, 0.7f) * 1.42f;
                break;
            case 2: // ASYMMETRIC SATURATION
                if (sample > 0.0f) {
                    sample = std::tanh(sample);
                } else {
                    sample = std::tanh(sample * 1.5f) * 0.8f;
                }
                break;
            default:
                break;
        }

        samples[i] = std::clamp(sample * makeup * 0.85f, -1.0f, 1.0f);
    }
}

// --- COMPRESSOR CALLBACK ---
void CompressorProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    if (!g_comp_enabled) return;

    float sample_rate = 48000.0f;
    float alpha_attack  = std::exp(-1.0f / (0.001f * g_comp_attack_ms * sample_rate));
    float alpha_release = std::exp(-1.0f / (0.001f * g_comp_release_ms * sample_rate));
    float makeup_linear = std::pow(10.0f, g_comp_makeup_db / 20.0f);

    for (unsigned int i = 0; i < frames; i++) {
        float left  = samples[i * 2];
        float right = samples[i * 2 + 1];

        float peak = std::max(std::abs(left), std::abs(right));

        if (peak > g_envelope) {
            g_envelope = alpha_attack * g_envelope + (1.0f - alpha_attack) * peak;
        } else {
            g_envelope = alpha_release * g_envelope + (1.0f - alpha_release) * peak;
        }

        float env_db = 20.0f * std::log10(std::max(g_envelope, 1e-6f));
        float control_db = 0.0f;
        if (env_db > g_comp_thresh_db) {
            float excess_db = env_db - g_comp_thresh_db;
            control_db = excess_db * (1.0f - (1.0f / g_comp_ratio));
        }

        float gr_linear = std::pow(10.0f, -control_db / 20.0f);
        float total_gain = gr_linear * makeup_linear;

        float out_left  = std::clamp(left * total_gain, -1.0f, 1.0f);
        float out_right = std::clamp(right * total_gain, -1.0f, 1.0f);

        samples[i * 2]     = out_left;
        samples[i * 2 + 1] = out_right;

        float out_peak = std::max(std::abs(out_left), std::abs(out_right));
        if (out_peak > g_out_envelope) {
            g_out_envelope = alpha_attack * g_out_envelope + (1.0f - alpha_attack) * out_peak;
        } else {
            g_out_envelope = alpha_release * g_out_envelope + (1.0f - alpha_release) * out_peak;
        }
    }
}

struct CombFilter {
    std::vector<float> buffer;
    size_t bufIdx = 0;
    float feedback = 0.5f;

    void init(size_t size) {
        buffer.assign(size, 0.0f);
        bufIdx = 0;
    }

    float process(float input) {
        float output = buffer[bufIdx];
        buffer[bufIdx] = input + (output * feedback);
        bufIdx = (bufIdx + 1) % buffer.size();
        return output;
    }
};

struct AllpassFilter {
    std::vector<float> buffer;
    size_t bufIdx = 0;
    float feedback = 0.5f;

    void init(size_t size) {
        buffer.assign(size, 0.0f);
        bufIdx = 0;
    }

    float process(float input) {
        float bufOut = buffer[bufIdx];
        float output = -input + bufOut;
        buffer[bufIdx] = input + (bufOut * feedback);
        bufIdx = (bufIdx + 1) % buffer.size();
        return output;
    }
};

static CombFilter g_combs[4];
static AllpassFilter g_allpass[2];
static bool g_reverb_inited = false;

void InitReverbDSP() {
    if (g_reverb_inited) return;

    g_combs[0].init(1116);
    g_combs[1].init(1188);
    g_combs[2].init(1276);
    g_combs[3].init(1356);

    g_allpass[0].init(225);
    g_allpass[1].init(556);
    g_reverb_inited = true;
}

void ReverbProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    if (!g_rev_enabled) return;

    InitReverbDSP();

    for (int i = 0; i < 4; i++) g_combs[i].feedback = std::clamp(g_rev_decay, 0.0f, 0.95f);

    for (unsigned int i = 0; i < frames; i++) {
        float left  = samples[i * 2];
        float right = samples[i * 2 + 1];
        float input = (left + right) * 0.5f;

        float combOut = 0.0f;
        for (int c = 0; c < 4; c++) {
            combOut += g_combs[c].process(input);
        }

        float diffOut = g_allpass[0].process(combOut * 0.25f);
        diffOut = g_allpass[1].process(diffOut);

        float wet = diffOut;
        samples[i * 2]     = std::clamp(left * (1.0f - g_rev_mix) + wet * g_rev_mix, -1.0f, 1.0f);
        samples[i * 2 + 1] = std::clamp(right * (1.0f - g_rev_mix) + wet * g_rev_mix, -1.0f, 1.0f);
    }
}

namespace VAudioNative {
    // --- BASIC DEVICE CONTROL ---
    Value native_init_audio(std::vector<Value>& args) {
        InitAudioDevice();
        SetMasterVolume(1.0f);
        return Value(IsAudioDeviceReady());
    }

    Value native_is_sound_playing(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        return Value(sound ? IsSoundPlaying(*sound) : false);
    }

    Value native_close_audio(std::vector<Value>& args) {
        CloseAudioDevice();
        return Value();
    }

    Value native_set_master_volume(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        SetMasterVolume((float)args[0].asFloat());
        return Value(true);
    }

    // --- SOUND ---
    Value native_load_sound(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("load_sound() requires path");
        std::string path = args[0].asString();
        Sound* sound = new Sound(LoadSound(path.c_str()));
        if (sound->frameCount == 0) {
            delete sound;
            throw std::runtime_error("Audio Error: Could not load sound at " + path);
        }
        return Value(reinterpret_cast<int64_t>(sound));
    }

    Value native_play_sound(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        if (sound) PlaySound(*sound);
        return Value(true);
    }

    Value native_set_sound_volume(std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        SetSoundVolume(*sound, (float)args[1].asFloat());
        return Value(true);
    }

    // --- STREAM ---
    Value native_play_stream(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("play_stream() requires path");
        std::string path = args[0].asString();
        
        Music* mPtr = new Music();
        *mPtr = LoadMusicStream(path.c_str());
        
        if (mPtr->stream.buffer == NULL) {
            delete mPtr;
            throw std::runtime_error("Audio Error: Failed to load stream at " + path);
        }

        mPtr->looping = true;
        PlayMusicStream(*mPtr);

        return Value(reinterpret_cast<int64_t>(mPtr));
    }

    Value native_update_stream(std::vector<Value>& args) {
        if (args.empty()) return Value();
        Music* m = reinterpret_cast<Music*>(args[0].asInt());
        if (m != nullptr && m->stream.buffer != nullptr) UpdateMusicStream(*m);
        return Value();
    }

    // --- DSP / SATURATION ---
    Value native_set_dsp_params(std::vector<Value>& args) {
        if (args.size() < 2) return Value();
        g_drive = (float)args[0].asFloat();
        g_mode = (int)args[1].asInt();
        return Value();
    }

    Value native_attach_saturation(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        if (sound != nullptr) {
            AttachAudioStreamProcessor(sound->stream, SaturationProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    // --- DSP / COMPRESSOR ---
    Value native_set_compressor_params(std::vector<Value>& args) {
        if (args.size() < 5) return Value(false);
        g_comp_thresh_db  = (float)args[0].asFloat();
        g_comp_ratio      = (float)args[1].asFloat();
        g_comp_attack_ms  = (float)args[2].asFloat();
        g_comp_release_ms = (float)args[3].asFloat();
        g_comp_makeup_db  = (float)args[4].asFloat();
        if (args.size() >= 6) g_comp_enabled = args[5].isTruthy();
        return Value(true);
    }

    Value native_attach_compressor(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        if (sound != nullptr) {
            AttachAudioStreamProcessor(sound->stream, CompressorProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    Value native_get_gain_reduction(std::vector<Value>& args) {
        float sample_rate = 48000.0f;
        
        float env_db = 20.0f * std::log10(std::max(g_envelope, 1e-6f));
        
        float gr_db = 0.0f;
        if (env_db > g_comp_thresh_db && g_comp_enabled) {
            float excess_db = env_db - g_comp_thresh_db;
            gr_db = excess_db * (1.0f - (1.0f / g_comp_ratio));
        }

        return Value(gr_db); // e.g. 0.0 when clean, 6.5 when compressing by 6.5 dB
    }

    Value native_get_rms(std::vector<Value>& args) {
        float rms_db = 20.0f * std::log10(std::max(g_out_envelope, 1e-6f));
        float rms_norm = std::clamp((rms_db + 60.0f) / 60.0f, 0.0f, 1.0f);
        return Value(rms_norm);
    }

    Value native_attach_reverb(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        if (sound != nullptr) {
            AttachAudioStreamProcessor(sound->stream, ReverbProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    Value native_set_reverb_params(std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        g_rev_decay = (float)args[0].asFloat(); // Decay (0.0 to 0.95)
        g_rev_mix   = (float)args[1].asFloat(); // Mix (0.0 to 1.0)
        if (args.size() >= 3) g_rev_enabled = args[2].isTruthy();
        return Value(true);
    }

    Value native_set_sound_3d(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("sound_3d() requires sound_ptr, listener_pos, source_pos, max_distance");

        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        if (!sound) return Value(false);

        std::vector<Value> lp = args[1].asList();
        std::vector<Value> sp = args[2].asList();
        float maxDist = (float)args[3].asFloat();
        float maxVol  = (args.size() > 4) ? (float)args[4].asFloat() : 1.0f;

        float dx = (float)lp[0].asFloat() - (float)sp[0].asFloat();
        float dy = (float)lp[1].asFloat() - (float)sp[1].asFloat();
        float dz = (float)lp[2].asFloat() - (float)sp[2].asFloat();
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);

        float vol = 0.0f;
        if (dist < maxDist) {
            float t = 1.0f - (dist / maxDist);
            vol = maxVol * (t * t);
        }

        SetSoundVolume(*sound, vol);
        return Value(vol);
    }
}

void setupVAudio(SymbolContainer& env, StringPool& pool) {
    const std::string& mod_name = "vaudio";
    if (env.find(mod_name) == env.end()) env[mod_name] = SymbolTable();
    auto& vaudio = env[mod_name];
    
    // Core
    vaudio[pool.intern("init_audio")]        = Value(VAudioNative::native_init_audio);
    vaudio[pool.intern("close_audio")]       = Value(VAudioNative::native_close_audio);
    vaudio[pool.intern("volume")]            = Value(VAudioNative::native_set_master_volume);
    
    // Sound
    vaudio[pool.intern("load_sound")]        = Value(VAudioNative::native_load_sound);
    vaudio[pool.intern("play_sound")]        = Value(VAudioNative::native_play_sound);
    vaudio[pool.intern("sound_volume")]      = Value(VAudioNative::native_set_sound_volume);
    vaudio[pool.intern("attach_saturator")]  = Value(VAudioNative::native_attach_saturation);
    
    // Compressor
    vaudio[pool.intern("attach_compressor")] = Value(VAudioNative::native_attach_compressor);
    vaudio[pool.intern("set_compressor")]    = Value(VAudioNative::native_set_compressor_params);
    vaudio[pool.intern("get_gr")]            = Value(VAudioNative::native_get_gain_reduction);

    // Stream
    vaudio[pool.intern("play_stream")]       = Value(VAudioNative::native_play_stream);
    vaudio[pool.intern("update_stream")]     = Value(VAudioNative::native_update_stream);
    vaudio[pool.intern("set_dsp")]           = Value(VAudioNative::native_set_dsp_params);
    vaudio[pool.intern("is_playing")]        = Value(VAudioNative::native_is_sound_playing);
    vaudio[pool.intern("get_rms")] = Value(VAudioNative::native_get_rms);

    // Reverb
    vaudio[pool.intern("attach_reverb")] = Value(VAudioNative::native_attach_reverb);
    vaudio[pool.intern("set_reverb")]    = Value(VAudioNative::native_set_reverb_params);

    // 3D
    vaudio[pool.intern("sound_3d")]          = Value(VAudioNative::native_set_sound_3d);
}