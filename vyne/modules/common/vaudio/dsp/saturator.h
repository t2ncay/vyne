#pragma once
#include <algorithm>
#include <cmath>

namespace VAudioDSP {

inline float g_drive = 0.5f;
inline int   g_mode  = 0;

inline float g_holdSample[2] = { 0.0f, 0.0f };
inline int   g_sampleCounter[2] = { 0, 0 };

inline float g_lpfState[2] = { 0.0f, 0.0f };

inline void SaturationProcessCallback(void *buffer, unsigned int frames) {
    if (g_drive <= 0.0f) return;
    
    float *samples = (float *)buffer;
    float gain = 1.0f + (g_drive * 3.0f);
    float makeup = 1.0f / std::sqrt(gain);

    for (unsigned int i = 0; i < frames * 2; i++) {
        int ch = i % 2; // 0 = Left, 1 = Right
        float sample = samples[i];

        switch (g_mode) {
            case 0: // SOFT TUBE
                sample = std::tanh(sample * gain);
                sample = sample * makeup * 0.85f;
                break;
            case 1: // HARD CLIP
                sample = std::clamp(sample * gain, -0.7f, 0.7f) * 1.42f;
                sample = sample * makeup * 0.85f;
                break;
            case 2: // ASYMMETRIC SATURATION
                sample *= gain;
                if (sample > 0.0f) {
                    sample = std::tanh(sample);
                } else {
                    sample = std::tanh(sample * 1.5f) * 0.8f;
                }
                sample = sample * makeup * 0.85f;
                break;
            case 3: // TAPE SATURATION / POLY TUBE
                sample *= gain;
                sample = sample - (1.0f / 3.0f) * sample * sample * sample;
                sample = sample * makeup * 0.85f;
                break;

            case 4: { // GAMEBOY 4-BIT DAC & DOWNSAMPLE
                // Drive parameter controls sample rate reduction factor (2 to 12x)
                int factor = 2 + static_cast<int>(g_drive * 10.0f);

                if (g_sampleCounter[ch] % factor == 0) {
                    float normalized = std::clamp((sample + 1.0f) * 0.5f, 0.0f, 1.0f);
                    float quantized4Bit = std::round(normalized * 15.0f);
                    
                    g_holdSample[ch] = (quantized4Bit / 7.5f) - 1.0f;
                }
                sample = g_holdSample[ch];
                g_sampleCounter[ch]++;

                float alpha = 0.45f;
                g_lpfState[ch] = g_lpfState[ch] + alpha * (sample - g_lpfState[ch]);
                sample = g_lpfState[ch];

                sample *= 0.85f;
                break;
            }
            default:
                break;
        }

        samples[i] = std::clamp(sample, -1.0f, 1.0f);
    }
}

} // namespace VAudioDSP