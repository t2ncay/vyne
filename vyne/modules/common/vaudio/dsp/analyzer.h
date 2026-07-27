#pragma once
#include <algorithm>
#include <cmath>
#include <atomic>

#include "shared_state.h"

namespace VAudioDSP {
    inline void AnalyzerProcessCallback(void *buffer, unsigned int frames) {
        float *samples = (float *)buffer;
        float sample_rate = 48000.0f;
        float alpha_attack = std::exp(-1.0f / (0.001f * 15.0f * sample_rate));
        float alpha_release = std::exp(-1.0f / (0.001f * 120.0f * sample_rate));

        for (unsigned int i = 0; i < frames; i++) {
            float left = samples[i * 2];
            float right = samples[i * 2 + 1];

            VAudioDSP::UpdateLUFSMeasurement(left, right);
            
            float peak = std::max(std::abs(left), std::abs(right));
            
            float current_env = g_analyzer_envelope.load();
            if (peak > current_env) {
                g_analyzer_envelope.store(alpha_attack * current_env + (1.0f - alpha_attack) * peak);
            } else {
                g_analyzer_envelope.store(alpha_release * current_env + (1.0f - alpha_release) * peak);
            }
        }
    }
}