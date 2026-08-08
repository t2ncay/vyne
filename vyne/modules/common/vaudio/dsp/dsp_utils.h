#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace VAudioDSP{
    struct WAVHeader {
        char chunkID[4] = {'R', 'I', 'F', 'F'};
        uint32_t chunkSize;
        char format[4] = {'W', 'A', 'V', 'E'};
        char subchunk1ID[4] = {'f', 'm', 't', ' '};
        uint32_t subchunk1Size = 16;
        uint16_t audioFormat = 3; // 3 = IEEE Float
        uint16_t numChannels = 2;
        uint32_t sampleRate = 48000;
        uint32_t byteRate = 48000 * 2 * sizeof(float);
        uint16_t blockAlign = 2 * sizeof(float);
        uint16_t bitsPerSample = 32;
        char subchunk2ID[4] = {'d', 'a', 't', 'a'};
        uint32_t subchunk2Size;
    };

    inline float limit_sample(float in_sample) {
        float threshold = 0.90f; // limit threshold (-0.9 dBFS)
        float abs_sample = std::abs(in_sample);

        if (abs_sample <= threshold) {
            return in_sample;
        }

        float excess = abs_sample - threshold;
        float compressed = threshold + (0.09f * std::tanh(excess / 0.09f));
        
        return (in_sample > 0.0f) ? compressed : -compressed;
    }

    inline void TruePeakLimiterCallback(void *buffer, unsigned int frames) {
        if (!buffer || frames == 0) return;
        float *samples = static_cast<float*>(buffer);

        static float gr_envelope = 1.0f;
        const float attack = 0.999f;
        const float release = 0.995f;

        for (unsigned int i = 0; i < frames; i++) {
            float left  = samples[i * 2];
            float right = samples[i * 2 + 1];

            float peak = std::max(std::abs(left), std::abs(right));
            
            float target_gain = 1.0f;
            if (peak > 0.95f) {
                target_gain = 0.95f / peak;
            }

            if (target_gain < gr_envelope) {
                gr_envelope = target_gain; // Clamp down instantly on transients
            } else {
                gr_envelope = gr_envelope * release + target_gain * (1.0f - release); // Recover smoothly
            }

            samples[i * 2]     = left * gr_envelope;
            samples[i * 2 + 1] = right * gr_envelope;
        }
    }
}
