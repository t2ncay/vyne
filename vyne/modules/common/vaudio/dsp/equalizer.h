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

inline Biquad g_eq_bands[7];
inline bool g_eq_enabled = true;

inline float g_bin_peaks[64] = {0.0f};
inline float g_bin_weights[64];
inline bool g_spectrum_initialized = false;

inline void InitSpectrumLookup() {
    if (g_spectrum_initialized) return;
    for (int bin = 0; bin < 64; bin++) {
        g_bin_weights[bin] = 1.0f + (1.0f - static_cast<float>(bin) / 63.0f) * 0.5f;
    }
    g_spectrum_initialized = true;
}

inline void EQProcessCallback(void *buffer, unsigned int frames) {
    InitSpectrumLookup();

    float *samples = (float *)buffer;

    const float attack = 0.35f;
    const float release = 0.08f;

    for (unsigned int i = 0; i < frames; i++) {
        float left  = samples[i * 2];
        float right = samples[i * 2 + 1];

        UpdateLUFSMeasurement(left, right);

        if (g_eq_enabled) {
            for (int b = 0; b < 7; b++) {
                left = g_eq_bands[b].process(left);
            }
            for (int b = 0; b < 7; b++) {
                right = g_eq_bands[b].process(right);
            }
        }

        float mono = std::abs(left + right) * 0.5f;
        for (int bin = 0; bin < 64; bin++) {
            float current_mag = mono * g_bin_weights[bin];

            if (current_mag > g_bin_peaks[bin]) {
                g_bin_peaks[bin] += (current_mag - g_bin_peaks[bin]) * attack;
            } else {
                g_bin_peaks[bin] -= (g_bin_peaks[bin] - current_mag) * release;
            }
        }

        samples[i * 2]     = std::clamp(left, -1.0f, 1.0f);
        samples[i * 2 + 1] = std::clamp(right, -1.0f, 1.0f);
    }

    for (int bin = 0; bin < 64; bin++) {
        g_fft_bins[bin].store(std::clamp(g_bin_peaks[bin], 0.0f, 1.0f), std::memory_order_relaxed);
    }
}
} // namespace VAudioDSP