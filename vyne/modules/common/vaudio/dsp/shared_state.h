#pragma once
#include <atomic>

namespace VAudioDSP {
    inline float g_sample_rate = 48000.0f;
    inline std::atomic<float> g_analyzer_envelope{0.0f};
    inline std::atomic<float> g_envelope         {0.0f}; 
    inline std::atomic<float> g_out_envelope     {0.0f};
    inline std::atomic<float> g_current_gr_db    {0.0f};
    inline std::atomic<float> g_fft_bins[64];
    
    inline float g_rms_sq_state{0.0f};
}