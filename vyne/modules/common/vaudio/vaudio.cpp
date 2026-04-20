#include "vaudio.h"
#include <cstring>

namespace VAudioNative {
    Value native_init_audio(std::vector<Value>& args) {
        InitAudioDevice();
        return Value(IsAudioDeviceReady());
    }

    Value native_load_sound(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("load_sound() requires a file path");
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
        if (sound) {
            PlaySound(*sound);
            return Value(true);
        }
        return Value(false);
    }

    Value native_set_master_volume(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        float volume = (float)args[0].asFloat();
        SetMasterVolume(volume);
        return Value(true);
    }

    Value native_set_sound_volume(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("set_sound_volume() requires sound_pointer and volume");
        
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        float volume = (float)args[1].asFloat();
        
        if (sound) {
            SetSoundVolume(*sound, volume);
            return Value(true);
        }
        return Value(false);
    }

    Value native_close_audio(std::vector<Value>& args) {
        CloseAudioDevice();
        return Value();
    }
}

void setupVAudio(SymbolContainer& env, StringPool& pool) {
    auto& vaudio = env["vaudio"];
    
    vaudio[pool.intern("init_audio")]   = Value(VAudioNative::native_init_audio);
    vaudio[pool.intern("load_sound")]   = Value(VAudioNative::native_load_sound);
    vaudio[pool.intern("play_sound")]   = Value(VAudioNative::native_play_sound);
    vaudio[pool.intern("close_audio")]  = Value(VAudioNative::native_close_audio);
    vaudio[pool.intern("volume")]       = Value(VAudioNative::native_set_master_volume);
    vaudio[pool.intern("sound_volume")] = Value(VAudioNative::native_set_sound_volume);
}