#include "vaudio.h"
#include <cstring>
#include <cmath>
#include <algorithm>

// digital sound processing parameters
float g_drive = 0.5f;
int   g_mode = 0;

void AudioProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    
    // Convert 0.0 - 1.0 drive parameter to gain multiplier (1.0x to 20.0x)
    float gain = 1.0f + (g_drive * 19.0f);

    for (unsigned int i = 0; i < frames * 2; i++) {
        float sample = samples[i] * gain;

        switch (g_mode) {
            case 0: // SOFT TUBE (tanh saturation)
                sample = std::tanh(sample);
                break;

            case 1: // HARD CLIP
                sample = std::clamp(sample, -0.8f, 0.8f) * 1.25f;
                break;

            case 2: // ASYMMETRIC SATURATION
                if (sample > 0.0f) {
                    sample = std::tanh(sample);
                } else {
                    sample = std::clamp(sample, -0.5f, 0.5f) * 2.0f;
                }
                break;

            default:
                break;
        }

        // Master output volume adjustment to prevent clipping
        samples[i] = sample * 0.7f;
    }
}

namespace VAudioNative {
    // --- BASIC DEVICE CONTROL ---
    Value native_init_audio(std::vector<Value>& args) {
        InitAudioDevice();
        SetMasterVolume(1.0f);
        
        SetAudioStreamBufferSizeDefault(4096); 
        
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

    // --- SOUND (RAM Based) ---
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

    // --- STREAM (Music/Long files) ---
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

        AttachAudioStreamProcessor(mPtr->stream, AudioProcessCallback);

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
            AttachAudioStreamProcessor(sound->stream, AudioProcessCallback);
            return Value(true);
        }
        return Value(false);
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
    vaudio[pool.intern("init_audio")]    = Value(VAudioNative::native_init_audio);
    vaudio[pool.intern("close_audio")]   = Value(VAudioNative::native_close_audio);
    vaudio[pool.intern("volume")]        = Value(VAudioNative::native_set_master_volume);
    
    // Sound
    vaudio[pool.intern("load_sound")]    = Value(VAudioNative::native_load_sound);
    vaudio[pool.intern("play_sound")]    = Value(VAudioNative::native_play_sound);
    vaudio[pool.intern("sound_volume")]  = Value(VAudioNative::native_set_sound_volume);
    vaudio[pool.intern("attach_saturator")] = Value(VAudioNative::native_attach_saturation);
    
    // Stream
    vaudio[pool.intern("play_stream")]   = Value(VAudioNative::native_play_stream);
    vaudio[pool.intern("update_stream")] = Value(VAudioNative::native_update_stream);
    vaudio[pool.intern("set_dsp")]       = Value(VAudioNative::native_set_dsp_params);
    vaudio[pool.intern("is_playing")] = Value(VAudioNative::native_is_sound_playing);

    // 3D
    vaudio[pool.intern("sound_3d")] = Value(VAudioNative::native_set_sound_3d);
}