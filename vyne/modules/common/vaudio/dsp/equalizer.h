#pragma once
#include <algorithm>
#include <cmath>
#include <atomic>

#include "shared_state.h"
#include "lufs.h"

namespace VAudioDSP {

struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void setPeaking(float freq, float Q, float gainDb) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > g_sample_rate * 0.49f) freq = g_sample_rate * 0.49f;

        float A = std::pow(10.0f, gainDb / 40.0f);
        float omega = 2.0f * 3.1415926535f * freq / g_sample_rate;
        float alpha = std::sin(omega) / (2.0f * std::max(Q, 0.1f));

        float norm = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / norm;
        b1 = (-2.0f * std::cos(omega)) / norm;
        b2 = (1.0f - alpha * A) / norm;
        a1 = (-2.0f * std::cos(omega)) / norm;
        a2 = (1.0f - alpha / A) / norm;
    }

    void setHighPass(float freq, float Q, float samplerate = 48000.0f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > samplerate * 0.49f) freq = samplerate * 0.49f;

        float omega = 2.0f * 3.1415926535f * freq / samplerate;
        float alpha = std::sin(omega) / (2.0f * std::max(Q, 0.1f));
        float cos_w = std::cos(omega);

        float a0 = 1.0f + alpha;
        b0 = ((1.0f + cos_w) / 2.0f) / a0;
        b1 = (-(1.0f + cos_w)) / a0;
        b2 = ((1.0f + cos_w) / 2.0f) / a0;
        a1 = (-2.0f * cos_w) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setLowPass(float freq, float Q, float samplerate = 48000.0f) {
        if (freq < 20.0f) freq = 20.0f;
        if (freq > samplerate * 0.49f) freq = samplerate * 0.49f;

        float omega = 2.0f * 3.1415926535f * freq / samplerate;
        float alpha = std::sin(omega) / (2.0f * std::max(Q, 0.1f));
        float cos_w = std::cos(omega);

        float a0 = 1.0f + alpha;
        b0 = ((1.0f - cos_w) / 2.0f) / a0;
        b1 = (1.0f - cos_w) / a0;
        b2 = ((1.0f - cos_w) / 2.0f) / a0;
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

struct EqualizerState {
    Biquad bands[7];
    bool enabled = true;

    void process(float* samples, unsigned int frames) {
        if (!enabled) return;

        for (unsigned int i = 0; i < frames; i++) {
            float left  = samples[i * 2];
            float right = samples[i * 2 + 1];

            for (int b = 0; b < 7; b++) {
                left = bands[b].process(left);
                right = bands[b].process(right);
            }

            samples[i * 2]     = std::clamp(left, -1.0f, 1.0f);
            samples[i * 2 + 1] = std::clamp(right, -1.0f, 1.0f);
        }
    }
};

inline Biquad g_eq_bands[7];          // Main 7-Band EQ Biquad Chain
inline Biquad g_bp_tracker[7];        // 7 Bandpass Filters for Peak Tracking
inline std::atomic<float> g_peak_envs[7]; // 7 Real-time Energy Envelopes
inline bool g_eq_enabled = true;      //[cite: 34]

inline void EQProcessCallback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;  //[cite: 34]

    // Initialize 7 ISO Center-Frequency Bandpass Trackers
    static bool filters_inited = false;
    if (!filters_inited) {
        float iso_freqs[7] = {60.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f, 14000.0f};
        for (int b = 0; b < 7; b++) {
            g_bp_tracker[b].setPeaking(iso_freqs[b], 1.2f, 6.0f);
        }
        filters_inited = true;
    }

    const float attack = 0.25f;
    const float release = 0.05f;
    static float local_envs[7] = {0.0f};

    for (unsigned int i = 0; i < frames; i++) {
        float left  = samples[i * 2];      //[cite: 34]
        float right = samples[i * 2 + 1];  //[cite: 34]

        UpdateLUFSMeasurement(left, right); //[cite: 34]

        // --- PROCESS ALL 7 BIQUAD BANDS IN SERIES ---
        if (g_eq_enabled) { //[cite: 34]
            for (int b = 0; b < 7; b++) {
                left  = g_eq_bands[b].process(left);   //[cite: 34]
                right = g_eq_bands[b].process(right);  //[cite: 34]
            }
        }

        float mono = (left + right) * 0.5f;

        // --- TRACK ENERGIES ACROSS ALL 7 BANDS ---
        for (int b = 0; b < 7; b++) {
            float band_sig = std::abs(g_bp_tracker[b].process(mono));
            local_envs[b] += (band_sig - local_envs[b]) * (band_sig > local_envs[b] ? attack : release);
        }

        samples[i * 2]     = std::clamp(left, -1.0f, 1.0f);   //[cite: 34]
        samples[i * 2 + 1] = std::clamp(right, -1.0f, 1.0f);  //[cite: 34]
    }

    // Store all 7 normalized energy levels atomically
    for (int b = 0; b < 7; b++) {
        g_peak_envs[b].store(local_envs[b], std::memory_order_relaxed);
    }
}

} // namespace VAudioDSP