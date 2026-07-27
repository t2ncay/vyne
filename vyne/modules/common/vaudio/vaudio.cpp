#include "vaudio.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void UpdateLUFSMeasurement(float left, float right);

// --- SOUND HANDLE WRAPPER ---
struct VAudioSoundHandle {
    Sound sound;
    bool is_paused = false;
    bool is_stopped = false;
};

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
bool  g_comp_auto_makeup = true;

// --- T4 PHOTOCELL STATE VARIABLES ---
static float g_opto_cap_fast = 0.0f; // Fast capacitor (short-term memory)
static float g_opto_cap_slow = 0.0f; // Slow capacitor (long-term memory tail)

// --- REVERB DSP PARAMETERS ---
float g_rev_decay = 0.5f;   // Decay time / Room Size (0.0 to 0.95)
float g_rev_mix   = 0.3f;   // Wet/Dry mix (0.0 = Dry, 1.0 = Wet)
bool  g_rev_enabled = true;

static float g_envelope = 0.0f; 
static float g_out_envelope = 0.0f;
static float g_current_gr_db = 0.0f;

struct WAVHeader {
    char chunkID[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;
    char format[4] = {'W', 'A', 'V', 'E'};
    char subchunk1ID[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 3; // 3 = IEEE Float
    uint16_t numChannels = 2;
    uint32_t sampleRate = 48000;
    uint32_t byteRate = 48000 * 2 * sizeof(float);
    uint16_t blockAlign = 2 * sizeof(float);
    uint16_t bitsPerSample = 32;
    char subchunk2ID[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size;
};

// --- SATURATOR CALLBACK ---
void SaturationProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    float gain = 1.0f + (g_drive * 3.0f);
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
            case 3: // TAPE SATURATION / POLY TUBE
                sample = sample - (1.0f / 3.0f) * sample * sample * sample;
                break;
            case 4: { // BITCRUSH
                float bits = 8.0f;
                float steps = std::pow(2.0f, bits);
                sample = std::round(sample * steps) / steps;
                break;
            }
            default:
                break;
        }

        samples[i] = std::clamp(sample * makeup * 0.85f, -1.0f, 1.0f);
    }
}

// --- COMPRESSOR CALLBACK ---
void CompressorProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;

    float sample_rate = 48000.0f;
    float alpha_attack  = std::exp(-1.0f / (0.001f * g_comp_attack_ms * sample_rate));
    float alpha_release = std::exp(-1.0f / (0.001f * g_comp_release_ms * sample_rate));
    
    float effective_makeup_db = g_comp_makeup_db;
    if (g_comp_auto_makeup && g_comp_thresh_db < 0.0f) {
        float ratio_factor = 1.0f - (1.0f / static_cast<float>(g_comp_ratio));
        float expected_gr_db = (-g_comp_thresh_db) * ratio_factor * 0.85f;
        effective_makeup_db += expected_gr_db;
    }

    float makeup_linear = std::pow(10.0f, effective_makeup_db / 20.0f);

    for (unsigned int i = 0; i < frames; i++) {
        float left  = samples[i * 2];
        float right = samples[i * 2 + 1];

        UpdateLUFSMeasurement(left, right);
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

        g_current_gr_db = g_comp_enabled ? control_db : 0.0f;

        if (!g_comp_enabled) {
            float clean_peak = peak;
            if (clean_peak > g_out_envelope) {
                g_out_envelope = alpha_attack * g_out_envelope + (1.0f - alpha_attack) * clean_peak;
            } else {
                g_out_envelope = alpha_release * g_out_envelope + (1.0f - alpha_release) * clean_peak;
            }
            continue;
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

// --- VALHALLA-STYLE MODULATED REVERB DSP ENGINE ---
struct OnePoleLP {
    float store = 0.0f;
    float process(float in, float coeff) {
        store = in * (1.0f - coeff) + store * coeff;
        return store;
    }
};

struct ModulatedDelay {
    std::vector<float> buffer;
    size_t writeIdx = 0;
    float phase = 0.0f;

    void init(size_t max_size) {
        buffer.assign(max_size, 0.0f);
        writeIdx = 0;
        phase = 0.0f;
    }

    float process(float input, float base_delay_samples, float mod_depth_samples, float mod_rate_hz) {
        if (buffer.empty()) return input;

        buffer[writeIdx] = input;

        phase += (2.0f * 3.14159265f * mod_rate_hz) / 48000.0f;
        if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;

        float current_delay = base_delay_samples + (std::sin(phase) * mod_depth_samples);
        current_delay = std::clamp(current_delay, 1.0f, (float)(buffer.size() - 2));

        float readPos = (float)writeIdx - current_delay;
        if (readPos < 0.0f) readPos += buffer.size();

        size_t idx0 = (size_t)readPos;
        size_t idx1 = (idx0 + 1) % buffer.size();
        float frac = readPos - (float)idx0;

        float out = buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;

        writeIdx = (writeIdx + 1) % buffer.size();
        return out;
    }
};

struct DiffuserAllpass {
    std::vector<float> buffer;
    size_t idx = 0;
    float feedback = 0.6f;

    void init(size_t size) {
        buffer.assign(size, 0.0f);
        idx = 0;
    }

    float process(float in) {
        if (buffer.empty()) return in;
        float bufOut = buffer[idx];
        float out = -in + bufOut;
        buffer[idx] = in + (bufOut * feedback);
        idx = (idx + 1) % buffer.size();
        return out;
    }
};

// Global Reverb State Variables
static DiffuserAllpass g_input_diffusers[4];
static ModulatedDelay g_loop_delays[4];
static OnePoleLP g_loop_dampers[4];
static ModulatedDelay g_predelay_line;

float g_rev_predelay_ms = 20.0f;
float g_rev_damping     = 0.4f;
static bool g_valhalla_reverb_inited = false;

void InitValhallaReverbDSP() {
    if (g_valhalla_reverb_inited) return;

    // Input transient diffusers
    g_input_diffusers[0].init(142);
    g_input_diffusers[1].init(107);
    g_input_diffusers[2].init(379);
    g_input_diffusers[3].init(277);

    // Prime-numbered delay line lengths for maximally dense feedback loops
    g_loop_delays[0].init(4800); // ~100ms max buffer
    g_loop_delays[1].init(4800);
    g_loop_delays[2].init(4800);
    g_loop_delays[3].init(4800);

    g_predelay_line.init(9600); // ~200ms predelay max

    g_valhalla_reverb_inited = true;
}

void ReverbProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    if (!g_rev_enabled) return;

    InitValhallaReverbDSP();

    float base_delays[4] = { 1357.0f, 1789.0f, 2143.0f, 2557.0f };
    
    float feedback_gain = std::clamp(g_rev_decay * 0.75f, 0.0f, 0.85f);
    float damp_coeff    = std::clamp(g_rev_damping, 0.05f, 0.92f);

    static float loop_node[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    for (unsigned int i = 0; i < frames; i++) {
        float in_l = samples[i * 2];
        float in_r = samples[i * 2 + 1];
        float mono_in = (in_l + in_r) * 0.5f;

        float predelay_samples = (g_rev_predelay_ms / 1000.0f) * 48000.0f;
        float delayed_in = g_predelay_line.process(mono_in * 0.5f, predelay_samples, 0.0f, 0.0f);

        float diff = delayed_in;
        for (int d = 0; d < 4; d++) {
            diff = g_input_diffusers[d].process(diff);
        }

        float sum = (loop_node[0] + loop_node[1] + loop_node[2] + loop_node[3]) * 0.25f;

        float next_node[4];
        for (int j = 0; j < 4; j++) {
            float in_to_delay = diff + (sum - loop_node[j]) * feedback_gain;
            
            in_to_delay = std::tanh(in_to_delay);

            in_to_delay = g_loop_dampers[j].process(in_to_delay, damp_coeff);

            float mod_rate = 0.5f + (j * 0.15f);
            next_node[j] = g_loop_delays[j].process(in_to_delay, base_delays[j], 2.0f, mod_rate);
        }

        for (int j = 0; j < 4; j++) loop_node[j] = next_node[j];

        float wet_l = (loop_node[0] - loop_node[2]) * 0.5f;
        float wet_r = (loop_node[1] - loop_node[3]) * 0.5f;

        float out_l = in_l * (1.0f - g_rev_mix) + wet_l * g_rev_mix;
        float out_r = in_r * (1.0f - g_rev_mix) + wet_r * g_rev_mix;

        samples[i * 2]     = std::tanh(out_l);
        samples[i * 2 + 1] = std::tanh(out_r);
    }
}

struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void setPeaking(float freq, float Q, float gainDb, float samplerate = 48000.0f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > samplerate * 0.49f) freq = samplerate * 0.49f;

        float A = powf(10.0f, gainDb / 40.0f);
        float omega = 2.0f * 3.1415926535f * freq / samplerate;
        float alpha = sinf(omega) / (2.0f * std::max(Q, 0.1f));

        float norm = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / norm;
        b1 = (-2.0f * cosf(omega)) / norm;
        b2 = (1.0f - alpha * A) / norm;
        a1 = (-2.0f * cosf(omega)) / norm;
        a2 = (1.0f - alpha / A) / norm;
    }

    void setHighPass(float freq, float Q, float samplerate = 48000.0f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > samplerate * 0.49f) freq = samplerate * 0.49f;

        float omega = 2.0f * 3.1415926535f * freq / samplerate;
        float alpha = sinf(omega) / (2.0f * std::max(Q, 0.1f));
        float cos_w = cosf(omega);

        float a0 = 1.0f + alpha;
        b0 = ((1.0f + cos_w) / 2.0f) / a0;
        b1 = (-(1.0f + cos_w)) / a0;
        b2 = ((1.0f + cos_w) / 2.0f) / a0;
        a1 = (-2.0f * cos_w) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    float process(float sample) {
        float out = b0 * sample + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = sample;
        y2 = y1; y1 = out;
        return out;
    }
};

static Biquad g_eq_bands[7];
static bool g_eq_enabled = true;

void EQProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    if (!g_eq_enabled) return;

    for (unsigned int i = 0; i < frames; i++) {
        float left  = samples[i * 2];
        float right = samples[i * 2 + 1];

        UpdateLUFSMeasurement(left, right);

        for (int b = 0; b < 7; b++) {
            left = g_eq_bands[b].process(left);
        }

        for (int b = 0; b < 7; b++) {
            right = g_eq_bands[b].process(right);
        }

        samples[i * 2]     = std::clamp(left, -1.0f, 1.0f);
        samples[i * 2 + 1] = std::clamp(right, -1.0f, 1.0f);
    }
}

static bool g_panning_enabled = true;

void PanningProcessCallback(void* buffer, unsigned int frames) {
    float *samples = (float *)buffer;

    if(!g_panning_enabled) return;

    for ( unsigned int i = 0; i < frames; i++ ){
        float left  = samples[i * 2];
        float right = samples[i * 2 + 1];
    }
}

// --- LUFS (ITU-R BS.1770) STATE & FILTERS ---
struct KWeightingFilter {
    float b0_hs = 1.53512485958697f, b1_hs   = -2.69169618940638f, b2_hs = 1.19839281085285f;
    float a1_hs = -1.69065929318241f, a2_hs  = 0.73248077421585f;
    float x1_hs_l = 0, x2_hs_l = 0, y1_hs_l  = 0, y2_hs_l = 0;
    float x1_hs_r = 0, x2_hs_r = 0, y1_hs_r  = 0, y2_hs_r = 0;

    float b0_hp = 1.0f, b1_hp = -2.0f, b2_hp = 1.0f;
    float a1_hp = -1.99004745483398f, a2_hp  = 0.99007225036621f;
    float x1_hp_l = 0, x2_hp_l = 0, y1_hp_l  = 0, y2_hp_l = 0;
    float x1_hp_r = 0, x2_hp_r = 0, y1_hp_r  = 0, y2_hp_r = 0;

    void process(float in_l, float in_r, float &out_l, float &out_r) {
        float hs_l = b0_hs * in_l + b1_hs * x1_hs_l + b2_hs * x2_hs_l - a1_hs * y1_hs_l - a2_hs * y2_hs_l;
        x2_hs_l = x1_hs_l; x1_hs_l = in_l; y2_hs_l = y1_hs_l; y1_hs_l = hs_l;

        out_l = b0_hp * hs_l + b1_hp * x1_hp_l + b2_hp * x2_hp_l - a1_hp * y1_hp_l - a2_hp * y2_hp_l;
        x2_hp_l = x1_hp_l; x1_hp_l = hs_l; y2_hp_l = y1_hp_l; y1_hp_l = out_l;

        float hs_r = b0_hs * in_r + b1_hs * x1_hs_r + b2_hs * x2_hs_r - a1_hs * y1_hs_r - a2_hs * y2_hs_r;
        x2_hs_r = x1_hs_r; x1_hs_r = in_r; y2_hs_r = y1_hs_r; y1_hs_r = hs_r;

        out_r = b0_hp * hs_r + b1_hp * x1_hp_r + b2_hp * x2_hp_r - a1_hp * y1_hp_r - a2_hp * y2_hp_r;
        x2_hp_r = x1_hp_r; x1_hp_r = hs_r; y2_hp_r = y1_hp_r; y1_hp_r = out_r;
    }
};

static KWeightingFilter g_k_filter;
static float g_lufs_energy_acc = 0.0f;
static unsigned int g_lufs_sample_count = 0;
static float g_current_lufs = -70.0f; // Momentary LUFS floor

void UpdateLUFSMeasurement(float left, float right) {
    float k_left = 0.0f, k_right = 0.0f;
    g_k_filter.process(left, right, k_left, k_right);

    g_lufs_energy_acc += (k_left * k_left) + (k_right * k_right);
    g_lufs_sample_count++;

    if (g_lufs_sample_count >= 19200) {
        float mean_square = g_lufs_energy_acc / (float)g_lufs_sample_count;
        
        if (mean_square > 1e-10f) {
            g_current_lufs = -0.691f + 10.0f * std::log10(mean_square);
        } else {
            g_current_lufs = -70.0f;
        }

        g_lufs_energy_acc = 0.0f;
        g_lufs_sample_count = 0;
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
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        return Value(handle ? IsSoundPlaying(handle->sound) : false);
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
        
        auto* handle = new VAudioSoundHandle();
        handle->sound = LoadSound(path.c_str());

        if (handle->sound.frameCount == 0) {
            delete handle;
            throw std::runtime_error("Audio Error: Could not load sound at " + path);
        }
        return Value(reinterpret_cast<int64_t>(handle));
    }

    Value native_play_sound(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle) {
            PlaySound(handle->sound);
            handle->is_paused = false;
            handle->is_stopped = false;
        }
        return Value(true);
    }

    Value native_pause_sound(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle) {
            PauseSound(handle->sound);
            handle->is_paused = true;
        }
        return Value(true);
    }

    Value native_resume_sound(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle) {
            ResumeSound(handle->sound);
            handle->is_paused = false;
            handle->is_stopped = false;
        }
        return Value(true);
    }

    Value native_is_sound_paused(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        return Value(handle ? handle->is_paused : false);
    }

    Value native_stop_sound(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle) {
            StopSound(handle->sound);
            handle->is_paused = false;
            handle->is_stopped = true;
        }
        return Value(true);
    }

    Value native_set_sound_volume(std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle) {
            SetSoundVolume(handle->sound, (float)args[1].asFloat());
        }
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
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle != nullptr) {
            AttachAudioStreamProcessor(handle->sound.stream, SaturationProcessCallback);
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
        if (args.size() >= 7) g_comp_auto_makeup = args[6].isTruthy();
        return Value(true);
    }

    Value native_attach_compressor(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle != nullptr) {
            AttachAudioStreamProcessor(handle->sound.stream, CompressorProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    Value native_get_gain_reduction(std::vector<Value>& args) {
        return Value(g_current_gr_db);
    }

    Value native_get_rms(std::vector<Value>& args) {
        float rms_db = 20.0f * std::log10(std::max(g_out_envelope, 1e-6f));
        float rms_norm = std::clamp((rms_db + 60.0f) / 60.0f, 0.0f, 1.0f);
        return Value(rms_norm);
    }

    Value native_attach_reverb(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle != nullptr) {
            AttachAudioStreamProcessor(handle->sound.stream, ReverbProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    Value native_set_reverb_params(std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        g_rev_decay = (float)args[0].asFloat();
        g_rev_mix   = (float)args[1].asFloat();
        if (args.size() >= 3) g_rev_predelay_ms = (float)args[2].asFloat();
        if (args.size() >= 4) g_rev_damping     = (float)args[3].asFloat();
        if (args.size() >= 5) g_rev_enabled     = args[4].isTruthy();
        return Value(true);
    }
    
    Value native_attach_eq(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle != nullptr) {
            AttachAudioStreamProcessor(handle->sound.stream, EQProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    Value native_set_eq_band(std::vector<Value>& args) {
        if (args.size() < 4) return Value(false);
        int bandIdx = (int)args[0].asInt();
        float freq  = (float)args[1].asFloat();
        float gain  = (float)args[2].asFloat();
        float q     = (float)args[3].asFloat();

        if (bandIdx >= 0 && bandIdx < 7) {
            if (bandIdx == 0) {
                g_eq_bands[bandIdx].setHighPass(freq, q);
            } else {
                g_eq_bands[bandIdx].setPeaking(freq, q, gain);
            }
        }
        return Value(true);
    }

    Value native_set_eq_enabled(std::vector<Value>& args) {
        if (!args.empty()) {
            g_eq_enabled = args[0].isTruthy();
        }
        return Value(true);
    }

    Value native_get_lufs(std::vector<Value>& args) {
        return Value((double)g_current_lufs);
    }

    Value native_set_sound_3d(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("sound_3d() requires sound_ptr, listener_pos, source_pos, max_distance");

        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (!handle) return Value(false);

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

        SetSoundVolume(handle->sound, vol);
        return Value(vol);
    }

    Value native_render_offline(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("render_offline() requires input_path and output_path");
        
        std::string input_path  = args[0].asString();
        std::string output_path = args[1].asString();

        Wave wave = LoadWave(input_path.c_str());
        if (wave.frameCount == 0) return Value(false);

        WaveFormat(&wave, 48000, 32, 2);

        float* samples = (float*)wave.data;
        uint64_t input_frames = wave.frameCount;

        unsigned int tail_frames = 48000 * 4; // 4 seconds tail padding
        uint64_t total_frames = input_frames + (g_rev_enabled ? tail_frames : 0);

        std::vector<float> render_buffer(total_frames * 2, 0.0f);
        std::memcpy(render_buffer.data(), samples, input_frames * 2 * sizeof(float));

        UnloadWave(wave);

        unsigned int block_size = 512;
        uint64_t processed = 0;

        while (processed < total_frames) {
            unsigned int current_frames = static_cast<unsigned int>(std::min<uint64_t>(block_size, total_frames - processed));
            float* block_ptr = render_buffer.data() + (processed * 2);

            if (g_eq_enabled)   EQProcessCallback(block_ptr, current_frames);
            if (g_comp_enabled) CompressorProcessCallback(block_ptr, current_frames);
            if (g_drive > 0.0f) SaturationProcessCallback(block_ptr, current_frames);
            if (g_rev_enabled)  ReverbProcessCallback(block_ptr, current_frames);

            processed += current_frames;
        }

        std::ofstream out(output_path, std::ios::binary);
        if (!out.is_open()) return Value(false);

        WAVHeader header;
        uint64_t pcm_data_size = total_frames * 2 * sizeof(float);
        
        header.subchunk2Size = static_cast<uint32_t>(pcm_data_size);
        header.chunkSize = static_cast<uint32_t>(36 + pcm_data_size);

        out.write(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
        out.write(reinterpret_cast<char*>(render_buffer.data()), pcm_data_size);
        out.close();

        return Value(true);
    }

    Value native_get_input_envelope(std::vector<Value>& args) {
        return Value((double)g_envelope);
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
    vaudio[pool.intern("get_env")] = Value(VAudioNative::native_get_input_envelope);

    // Stream
    vaudio[pool.intern("play_stream")]       = Value(VAudioNative::native_play_stream);
    vaudio[pool.intern("update_stream")]     = Value(VAudioNative::native_update_stream);
    vaudio[pool.intern("set_dsp")]           = Value(VAudioNative::native_set_dsp_params);
    vaudio[pool.intern("is_playing")]        = Value(VAudioNative::native_is_sound_playing);
    vaudio[pool.intern("get_rms")]           = Value(VAudioNative::native_get_rms);
    vaudio[pool.intern("get_lufs")]          = Value(VAudioNative::native_get_lufs);

    // Reverb
    vaudio[pool.intern("attach_reverb")]     = Value(VAudioNative::native_attach_reverb);
    vaudio[pool.intern("set_reverb")]        = Value(VAudioNative::native_set_reverb_params);

    // Equalizer
    vaudio[pool.intern("attach_eq")]         = Value(VAudioNative::native_attach_eq);
    vaudio[pool.intern("set_eq")]            = Value(VAudioNative::native_set_eq_band);
    vaudio[pool.intern("enable_eq")]         = Value(VAudioNative::native_set_eq_enabled);

    // 3D
    vaudio[pool.intern("sound_3d")]          = Value(VAudioNative::native_set_sound_3d);

    // Render
    vaudio[pool.intern("render_offline")]    = Value(VAudioNative::native_render_offline);

    // Audio state
    vaudio[pool.intern("pause_sound")]       = Value(VAudioNative::native_pause_sound);
    vaudio[pool.intern("resume_sound")]      = Value(VAudioNative::native_resume_sound);
    vaudio[pool.intern("is_paused")]         = Value(VAudioNative::native_is_sound_paused);
    vaudio[pool.intern("stop_sound")]        = Value(VAudioNative::native_stop_sound);
}