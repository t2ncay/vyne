#pragma once
#include <algorithm>
#include <cmath>
#include <atomic>

#include "shared_state.h"
#include "lufs.h"

namespace VAudioDSP {
    struct BPMDetector {
        float sample_rate = 48000.0f;
        float energy_buffer[43] = {0.0f}; // ~0.5s of historical frame energies
        int energy_idx = 0;
        
        uint64_t total_samples_processed = 0;
        uint64_t last_beat_sample = 0;
        
        std::vector<float> interval_history; // Stored beat intervals in seconds
        float calculated_bpm = 120.0f;
        
        float sensitivity = 1.35f; // Threshold multiplier for peak detection
        uint64_t min_beat_distance_samples = 14400; // ~300ms min spacing (~200 Max BPM)

        void processBlock(const float* samples, unsigned int frames) {
            if (frames == 0) return;

            float frame_energy = 0.0f;
            for (unsigned int i = 0; i < frames; i++) {
                float mono = (samples[i * 2] + samples[i * 2 + 1]) * 0.5f;
                frame_energy += mono * mono;
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
                    
                    if (interval_sec >= 0.33f && interval_sec <= 1.0f) {
                        interval_history.push_back(interval_sec);
                        if (interval_history.size() > 16) {
                            interval_history.erase(interval_history.begin());
                        }

                        std::vector<float> sorted_intervals = interval_history;
                        std::sort(sorted_intervals.begin(), sorted_intervals.end());
                        float median_interval = sorted_intervals[sorted_intervals.size() / 2];

                        if (median_interval > 0.0f) {
                            calculated_bpm = 60.0f / median_interval;
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