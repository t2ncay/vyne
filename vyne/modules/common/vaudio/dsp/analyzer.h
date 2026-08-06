#pragma once
#include <algorithm>
#include <cmath>
#include <atomic>
#include <vector>

#include "shared_state.h"
#include "lufs.h"

namespace VAudioDSP {
    struct BPMDetector {
        float sample_rate = 48000.0f;
        float energy_buffer[43] = {0.0f}; // ~0.5s of historical frame energies
        int energy_idx = 0;
        
        uint64_t total_samples_processed = 0;
        uint64_t last_beat_sample = 0;
        
        // Fixed ring-buffer for audio thread safety
        float interval_history[16] = {0.0f};
        int interval_count = 0;
        int interval_idx = 0;

        float calculated_bpm = 120.0f;
        
        float sensitivity = 1.30f; // Adjusted sensitivity
        uint64_t min_beat_distance_samples = 14400; // ~300ms min spacing (~200 Max BPM)

        // Low-pass state to isolate kick drum / bass transients
        float lpf_state = 0.0f;

        void processBlock(const float* samples, unsigned int frames) {
            if (frames == 0) return;

            float frame_energy = 0.0f;
            
            // Simple 150Hz One-Pole LPF coefficient at 48kHz
            float dt = 1.0f / sample_rate;
            float rc = 1.0f / (2.0f * 3.14159265f * 150.0f);
            float alpha = dt / (rc + dt);

            for (unsigned int i = 0; i < frames; i++) {
                float mono = (samples[i * 2] + samples[i * 2 + 1]) * 0.5f;
                // Low-pass filter the mono sample to focus on low-end beats
                lpf_state += alpha * (mono - lpf_state);
                frame_energy += lpf_state * lpf_state;
            }
            frame_energy /= (float)frames;

            float avg_energy = 0.0f;
            for (int i = 0; i < 43; i++) avg_energy += energy_buffer[i];
            avg_energy /= 43.0f;

            total_samples_processed += frames;
            uint64_t samples_since_last = total_samples_processed - last_beat_sample;

            if (frame_energy > (avg_energy * sensitivity) && samples_since_last > min_beat_distance_samples) {
                if (last_beat_sample > 0) {
                    float interval_sec = (float)samples_since_last / sample_rate;
                    
                    // Allow 0.30s (200 BPM) to 1.20s (50 BPM) intervals
                    if (interval_sec >= 0.30f && interval_sec <= 1.20f) {
                        
                        // Push into ring buffer (no std::vector allocation)
                        interval_history[interval_idx] = interval_sec;
                        interval_idx = (interval_idx + 1) % 16;
                        if (interval_count < 16) interval_count++;

                        // Sort active history window
                        float temp_intervals[16];
                        for (int k = 0; k < interval_count; k++) {
                            temp_intervals[k] = interval_history[k];
                        }
                        std::sort(temp_intervals, temp_intervals + interval_count);

                        float median_interval = temp_intervals[interval_count / 2];

                        if (median_interval > 0.0f) {
                            float raw_bpm = 60.0f / median_interval;
                            // Smooth BPM transitions
                            calculated_bpm = calculated_bpm * 0.7f + raw_bpm * 0.3f;
                        }
                    }
                }
                last_beat_sample = total_samples_processed;
            }

            energy_buffer[energy_idx] = frame_energy;
            energy_idx = (energy_idx + 1) % 43;
        }
    };

    inline void AnalyzerProcessCallback(void *buffer, unsigned int frames) {
        float *samples = (float *)buffer;
        float alpha_attack = std::exp(-1.0f / (0.001f * 15.0f * g_sample_rate));
        float alpha_release = std::exp(-1.0f / (0.001f * 120.0f * g_sample_rate));

        for (unsigned int i = 0; i < frames; i++) {
            float left = samples[i * 2];
            float right = samples[i * 2 + 1];

            UpdateLUFSMeasurement(left, right);
            
            float peak = std::max(std::abs(left), std::abs(right));
            
            float current_env = g_analyzer_envelope.load(std::memory_order_relaxed);
            if (peak > current_env) {
                g_analyzer_envelope.store(alpha_attack * current_env + (1.0f - alpha_attack) * peak);
            } else {
                g_analyzer_envelope.store(alpha_release * current_env + (1.0f - alpha_release) * peak);
            }
        }
    }
}