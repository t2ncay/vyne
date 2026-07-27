#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace VAudioDSP {

    struct OnePoleLP {
        float store = 0.0f;
        float process(float in, float coeff) {
            store = in * (1.0f - coeff) + store * coeff;
            return store;
        }
    };

    struct ModulatedDelay {
        std::vector<float> buffer;
        size_t writeIdx = 0;
        float phase = 0.0f;

        void init(size_t max_size) {
            buffer.assign(max_size, 0.0f);
            writeIdx = 0;
            phase = 0.0f;
        }

        float process(float input, float base_delay_samples, float mod_depth_samples, float mod_rate_hz) {
            if (buffer.empty()) return input;

            buffer[writeIdx] = input;

            phase += (2.0f * 3.14159265f * mod_rate_hz) / 48000.0f;
            if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;

            float current_delay = base_delay_samples + (std::sin(phase) * mod_depth_samples);
            current_delay = std::clamp(current_delay, 1.0f, (float)(buffer.size() - 2));

            float readPos = (float)writeIdx - current_delay;
            if (readPos < 0.0f) readPos += buffer.size();

            size_t idx0 = (size_t)readPos;
            size_t idx1 = (idx0 + 1) % buffer.size();
            float frac = readPos - (float)idx0;

            float out = buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;

            writeIdx = (writeIdx + 1) % buffer.size();
            return out;
        }
    };

    struct DiffuserAllpass {
        std::vector<float> buffer;
        size_t idx = 0;
        float feedback = 0.6f;

        void init(size_t size) {
            buffer.assign(size, 0.0f);
            idx = 0;
        }

        float process(float in) {
            if (buffer.empty()) return in;
            float bufOut = buffer[idx];
            float out = -in + bufOut;
            buffer[idx] = in + (bufOut * feedback);
            idx = (idx + 1) % buffer.size();
            return out;
        }
    };

    // Reverb State & Callback
    inline float g_rev_decay = 0.5f;
    inline float g_rev_mix   = 0.3f;
    inline float g_rev_predelay_ms = 20.0f;
    inline float g_rev_damping     = 0.4f;
    inline bool  g_rev_enabled     = true;

    static DiffuserAllpass g_input_diffusers[4];
    static ModulatedDelay  g_loop_delays[4];
    static OnePoleLP       g_loop_dampers[4];
    static ModulatedDelay  g_predelay_line;
    static bool            g_valhalla_reverb_inited = false;

    inline void InitValhallaReverbDSP() {
        if (g_valhalla_reverb_inited) return;

        g_input_diffusers[0].init(142);
        g_input_diffusers[1].init(107);
        g_input_diffusers[2].init(379);
        g_input_diffusers[3].init(277);

        g_loop_delays[0].init(4800);
        g_loop_delays[1].init(4800);
        g_loop_delays[2].init(4800);
        g_loop_delays[3].init(4800);

        g_predelay_line.init(9600);

        g_valhalla_reverb_inited = true;
    }

    inline void ReverbProcessCallback(void *buffer, unsigned int frames) {
        float *samples = (float *)buffer;
        if (!g_rev_enabled) return;

        InitValhallaReverbDSP();

        float base_delays[4] = { 1357.0f, 1789.0f, 2143.0f, 2557.0f };
        float feedback_gain = std::clamp(g_rev_decay * 0.75f, 0.0f, 0.85f);
        float damp_coeff    = std::clamp(g_rev_damping, 0.05f, 0.92f);

        static float loop_node[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        for (unsigned int i = 0; i < frames; i++) {
            float in_l = samples[i * 2];
            float in_r = samples[i * 2 + 1];
            float mono_in = (in_l + in_r) * 0.5f;

            float predelay_samples = (g_rev_predelay_ms / 1000.0f) * 48000.0f;
            float delayed_in = g_predelay_line.process(mono_in * 0.5f, predelay_samples, 0.0f, 0.0f);

            float diff = delayed_in;
            for (int d = 0; d < 4; d++) {
                diff = g_input_diffusers[d].process(diff);
            }

            float sum = (loop_node[0] + loop_node[1] + loop_node[2] + loop_node[3]) * 0.25f;

            float next_node[4];
            for (int j = 0; j < 4; j++) {
                float in_to_delay = diff + (sum - loop_node[j]) * feedback_gain;
                in_to_delay = std::tanh(in_to_delay);
                in_to_delay = g_loop_dampers[j].process(in_to_delay, damp_coeff);

                float mod_rate = 0.5f + (j * 0.15f);
                next_node[j] = g_loop_delays[j].process(in_to_delay, base_delays[j], 2.0f, mod_rate);
            }

            for (int j = 0; j < 4; j++) loop_node[j] = next_node[j];

            float wet_l = (loop_node[0] - loop_node[2]) * 0.5f;
            float wet_r = (loop_node[1] - loop_node[3]) * 0.5f;

            float out_l = in_l * (1.0f - g_rev_mix) + wet_l * g_rev_mix;
            float out_r = in_r * (1.0f - g_rev_mix) + wet_r * g_rev_mix;

            samples[i * 2]     = std::tanh(out_l);
            samples[i * 2 + 1] = std::tanh(out_r);
        }
    }

}