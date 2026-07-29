#pragma once
#include <algorithm>
#include <cmath>

namespace VAudioDSP {

// --- LUFS (ITU-R BS.1770) STATE & FILTERS ---
struct KWeightingFilter {
    float b0_hs = 1.53512485958697f, b1_hs = -2.69169618940638f, b2_hs = 1.19839281085285f;
    float a1_hs = -1.69065929318241f, a2_hs = 0.73248077421585f;
    float x1_hs_l = 0, x2_hs_l = 0, y1_hs_l = 0, y2_hs_l = 0;
    float x1_hs_r = 0, x2_hs_r = 0, y1_hs_r = 0, y2_hs_r = 0;

    float b0_hp = 1.0f, b1_hp = -2.0f, b2_hp = 1.0f;
    float a1_hp = -1.99004745483398f, a2_hp = 0.99007225036621f;
    float x1_hp_l = 0, x2_hp_l = 0, y1_hp_l = 0, y2_hp_l = 0;
    float x1_hp_r = 0, x2_hp_r = 0, y1_hp_r = 0, y2_hp_r = 0;

    void process(float in_l, float in_r, float &out_l, float &out_r) {
        float hs_l = b0_hs * in_l + b1_hs * x1_hs_l + b2_hs * x2_hs_l - a1_hs * y1_hs_l - a2_hs * y2_hs_l;
        x2_hs_l = x1_hs_l; x1_hs_l = in_l; y2_hs_l = y1_hs_l; y1_hs_l = hs_l;

        out_l = b0_hp * hs_l + b1_hp * x1_hp_l + b2_hp * x2_hp_l - a1_hp * y1_hp_l - a2_hp * y2_hp_l;
        x2_hp_l = x1_hp_l; x1_hp_l = hs_l; y2_hp_l = y1_hp_l; y1_hp_l = out_l;

        float hs_r = b0_hs * in_r + b1_hs * x1_hs_r + b2_hs * x2_hs_r - a1_hs * y1_hs_r - a2_hs * y2_hs_r;
        x2_hs_r = x1_hs_r; x1_hs_r = in_r; y2_hs_r = y1_hs_r; y1_hs_r = hs_r;

        out_r = b0_hp * hs_r + b1_hp * x1_hp_r + b2_hp * x2_hp_r - a1_hp * y1_hp_r - a2_hp * y2_hp_r;
        x2_hp_r = x1_hp_r; x1_hp_r = hs_r; y2_hp_r = y1_hp_r; y1_hp_r = out_r;
    }
};

inline KWeightingFilter g_k_filter;
inline float g_lufs_energy_acc = 0.0f;
inline unsigned int g_lufs_sample_count = 0;
inline std::atomic<float> g_current_lufs = -70.0f;

static inline void UpdateLUFSMeasurement(float left, float right) {
    float k_left = 0.0f, k_right = 0.0f;
    g_k_filter.process(left, right, k_left, k_right);

    g_lufs_energy_acc += (k_left * k_left) + (k_right * k_right);
    g_lufs_sample_count++;

    if (g_lufs_sample_count >= 19200) { // ~400ms window at 48kHz
        float mean_square = g_lufs_energy_acc / (float)g_lufs_sample_count;
        
        if (mean_square > 1e-10f) {
            g_current_lufs.store(-0.691f + 10.0f * std::log10(mean_square));
        } else {
            g_current_lufs.store(-70.0f);
        }

        g_lufs_energy_acc = 0.0f;
        g_lufs_sample_count = 0;
    }
}

} // namespace VAudioDSP