#pragma once
#include <algorithm>
#include <cmath>

#include "shared_state.h"

namespace VAudioDSP {

// forward declaration from DspUtils or LUFS module
void UpdateLUFSMeasurement(float left, float right);

inline float g_comp_thresh_db   = -12.0f; // -60 dB to 0 dB
inline float g_comp_ratio       = 4.0f;   // 1.0 to 20.0
inline float g_comp_attack_ms   = 15.0f;  // 0.1 ms to 100 ms
inline float g_comp_release_ms  = 120.0f; // 10 ms to 1000 ms
inline float g_comp_makeup_db   = 0.0f;   // 0 dB to 24 dB
inline bool  g_comp_enabled     = true;
inline bool  g_comp_auto_makeup = true;

inline int   g_comp_detection_mode = 1; 
inline float g_comp_rms_window_ms  = 30.0f; // Typical RMS window (10ms - 50ms)

inline void CompressorProcessCallback(void *buffer, unsigned int frames) {
    if(!buffer || frames == 0) return;

    float *samples = (float *)buffer;

    float sample_rate = g_sample_rate; 
    
    float alpha_attack  = std::exp(-1.0f / (0.001f * g_comp_attack_ms * sample_rate));
    float alpha_release = std::exp(-1.0f / (0.001f * g_comp_release_ms * sample_rate));
    float alpha_rms     = std::exp(-1.0f / (0.001f * g_comp_rms_window_ms * sample_rate));

    float effective_makeup_db = g_comp_makeup_db;
    if (g_comp_auto_makeup && g_comp_thresh_db < 0.0f) {
        float ratio_factor = 1.0f - (1.0f / static_cast<float>(g_comp_ratio));
        float expected_gr_db = (-g_comp_thresh_db) * ratio_factor * 0.85f;
        effective_makeup_db += expected_gr_db;
    }
    float makeup_linear = std::pow(10.0f, effective_makeup_db / 20.0f);

    float local_env     = g_envelope.load(std::memory_order_relaxed);
    float local_out_env = g_out_envelope.load(std::memory_order_relaxed);
    float last_gr_db    = 0.0f;

    for (unsigned int i = 0; i < frames; i++) {
        float left  = samples[i * 2];
        float right = samples[i * 2 + 1];

        UpdateLUFSMeasurement(left, right);

        float detector_signal = 0.0f;
        if (g_comp_detection_mode == 0) {
            detector_signal = std::max(std::abs(left), std::abs(right));
        } else {
            float power = 0.5f * (left * left + right * right);
            g_rms_sq_state = alpha_rms * g_rms_sq_state + (1.0f - alpha_rms) * power;
            detector_signal = std::sqrt(std::max(g_rms_sq_state, 1e-12f));
        }

        if (detector_signal > local_env) {
            local_env = alpha_attack * local_env + (1.0f - alpha_attack) * detector_signal;
        } else {
            local_env = alpha_release * local_env + (1.0f - alpha_release) * detector_signal;
        }

        float env_db = 20.0f * std::log10(std::max(local_env, 1e-6f));
        float control_db = 0.0f;
        
        if (env_db > g_comp_thresh_db) {
            float excess_db = env_db - g_comp_thresh_db;
            control_db = excess_db * (1.0f - (1.0f / g_comp_ratio));
        }

        last_gr_db = g_comp_enabled ? control_db : 0.0f;

        // Bypass Path
        if (!g_comp_enabled) {
            float clean_peak = std::max(std::abs(left), std::abs(right));
            if (clean_peak > local_out_env) {
                local_out_env = alpha_attack * local_out_env + (1.0f - alpha_attack) * clean_peak;
            } else {
                local_out_env = alpha_release * local_out_env + (1.0f - alpha_release) * clean_peak;
            }
            continue;
        }

        // Processing Path
        float gr_linear  = std::pow(10.0f, -control_db / 20.0f);
        float total_gain = gr_linear * makeup_linear;

        float out_left  = std::clamp(left * total_gain, -1.0f, 1.0f);
        float out_right = std::clamp(right * total_gain, -1.0f, 1.0f);

        samples[i * 2]     = out_left;
        samples[i * 2 + 1] = out_right;

        float out_peak = std::max(std::abs(out_left), std::abs(out_right));
        if (out_peak > local_out_env) {
            local_out_env = alpha_attack * local_out_env + (1.0f - alpha_attack) * out_peak;
        } else {
            local_out_env = alpha_release * local_out_env + (1.0f - alpha_release) * out_peak;
        }
    }

    if (local_env < 1e-6f) local_env = 0.0f;
    if (local_out_env < 1e-6f) local_out_env = 0.0f;

    g_envelope.store(local_env, std::memory_order_relaxed);
    g_out_envelope.store(local_out_env, std::memory_order_relaxed);
    g_current_gr_db.store(last_gr_db, std::memory_order_relaxed);
}
}