#pragma once
#include <algorithm>
#include <cmath>

namespace VAudioDSP {

inline float g_drive = 0.5f;
inline int   g_mode  = 0;

inline void SaturationProcessCallback(void *buffer, unsigned int frames) {
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

} // namespace VAudioDSP