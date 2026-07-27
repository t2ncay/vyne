#include "vaudio.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "dsp/equalizer.h"
#include "dsp/compressor.h"
#include "dsp/saturator.h"
#include "dsp/reverb.h"
#include "dsp/lufs.h"
#include "dsp/dsp_utils.h"
#include "dsp/analyzer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- SOUND HANDLE WRAPPER ---
struct VAudioSoundHandle {
    Sound sound;
    bool is_paused = false;
    bool is_stopped = false;
};

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
        VAudioDSP::g_drive = (float)args[0].asFloat();
        VAudioDSP::g_mode = (int)args[1].asInt();
        return Value();
    }

    Value native_attach_saturation(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle != nullptr) {
            AttachAudioStreamProcessor(handle->sound.stream, VAudioDSP::SaturationProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    // --- DSP / COMPRESSOR ---
    Value native_set_compressor_params(std::vector<Value>& args) {
        if (args.size() < 5) return Value(false);
        VAudioDSP::g_comp_thresh_db  = (float)args[0].asFloat();
        VAudioDSP::g_comp_ratio      = (float)args[1].asFloat();
        VAudioDSP::g_comp_attack_ms  = (float)args[2].asFloat();
        VAudioDSP::g_comp_release_ms = (float)args[3].asFloat();
        VAudioDSP::g_comp_makeup_db  = (float)args[4].asFloat();
        if (args.size() >= 6) VAudioDSP::g_comp_enabled = args[5].isTruthy();
        if (args.size() >= 7) VAudioDSP::g_comp_auto_makeup = args[6].isTruthy();
        return Value(true);
    }

    Value native_attach_compressor(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle != nullptr) {
            AttachAudioStreamProcessor(handle->sound.stream, VAudioDSP::CompressorProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    Value native_get_gain_reduction(std::vector<Value>& args) {
        return Value(VAudioDSP::g_current_gr_db);
    }

    Value native_get_rms(std::vector<Value>& args) {
        float rms_db = 20.0f * std::log10(std::max(VAudioDSP::g_out_envelope.load(), 1e-6f));
        float rms_norm = std::clamp((rms_db + 60.0f) / 60.0f, 0.0f, 1.0f);
        return Value(rms_norm);
    }

    Value native_attach_reverb(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle != nullptr) {
            AttachAudioStreamProcessor(handle->sound.stream, VAudioDSP::ReverbProcessCallback);
            return Value(true);
        }
        return Value(false);
    }

    Value native_set_reverb_params(std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);
        VAudioDSP::g_rev_decay = (float)args[0].asFloat();
        VAudioDSP::g_rev_mix   = (float)args[1].asFloat();
        if (args.size() >= 3) VAudioDSP::g_rev_predelay_ms = (float)args[2].asFloat();
        if (args.size() >= 4) VAudioDSP::g_rev_damping     = (float)args[3].asFloat();
        if (args.size() >= 5) VAudioDSP::g_rev_enabled     = args[4].isTruthy();
        return Value(true);
    }
    
    Value native_attach_eq(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle != nullptr) {
            AttachAudioStreamProcessor(handle->sound.stream, VAudioDSP::EQProcessCallback);
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
                VAudioDSP::g_eq_bands[bandIdx].setHighPass(freq, q);
            } else {
                VAudioDSP::g_eq_bands[bandIdx].setPeaking(freq, q, gain);
            }
        }
        return Value(true);
    }

    Value native_set_eq_enabled(std::vector<Value>& args) {
        if (!args.empty()) {
            VAudioDSP::g_eq_enabled = args[0].isTruthy();
        }
        return Value(true);
    }

    Value native_get_lufs(std::vector<Value>& args) {
        return Value((double)VAudioDSP::g_current_lufs.load());
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
        uint64_t total_frames = input_frames + (VAudioDSP::g_rev_enabled ? tail_frames : 0);

        std::vector<float> render_buffer(total_frames * 2, 0.0f);
        std::memcpy(render_buffer.data(), samples, input_frames * 2 * sizeof(float));

        UnloadWave(wave);

        unsigned int block_size = 512;
        uint64_t processed = 0;

        while (processed < total_frames) {
            unsigned int current_frames = static_cast<unsigned int>(std::min<uint64_t>(block_size, total_frames - processed));
            float* block_ptr = render_buffer.data() + (processed * 2);

            if (VAudioDSP::g_eq_enabled)   VAudioDSP::EQProcessCallback(block_ptr, current_frames);
            if (VAudioDSP::g_comp_enabled) VAudioDSP::CompressorProcessCallback(block_ptr, current_frames);
            if (VAudioDSP::g_drive > 0.0f) VAudioDSP::SaturationProcessCallback(block_ptr, current_frames);
            if (VAudioDSP::g_rev_enabled)  VAudioDSP::ReverbProcessCallback(block_ptr, current_frames);

            processed += current_frames;
        }

        std::ofstream out(output_path, std::ios::binary);
        if (!out.is_open()) return Value(false);

        VAudioDSP::WAVHeader header;
        uint64_t pcm_data_size = total_frames * 2 * sizeof(float);
        
        header.subchunk2Size = static_cast<uint32_t>(pcm_data_size);
        header.chunkSize = static_cast<uint32_t>(36 + pcm_data_size);

        out.write(reinterpret_cast<char*>(&header), sizeof(VAudioDSP::WAVHeader));
        out.write(reinterpret_cast<char*>(render_buffer.data()), pcm_data_size);
        out.close();

        return Value(true);
    }

    Value native_get_input_envelope(std::vector<Value>& args) {
        return Value((double)VAudioDSP::g_envelope);
    }

    Value native_get_analyzer_envelope(std::vector<Value>& args) {
        return Value((double)VAudioDSP::g_analyzer_envelope.load());
    }

    Value native_attach_analyzer(std::vector<Value>& args) {
        auto* handle = reinterpret_cast<VAudioSoundHandle*>(args[0].asInt());
        if (handle) {
            AttachAudioStreamProcessor(handle->sound.stream, VAudioDSP::AnalyzerProcessCallback);
            return Value(true);
        }
        return Value(false);
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
    vaudio[pool.intern("get_env")]           = Value(VAudioNative::native_get_input_envelope);
    vaudio[pool.intern("get_analyzer_env")]  = Value(VAudioNative::native_get_analyzer_envelope);

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

    // Analyzer
    vaudio[pool.intern("attach_analyzer")]   = Value(VAudioNative::native_attach_analyzer);

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