#include "vglib_common.h"

namespace VGLibNative {

Value native_draw_line(std::vector<Value>& args) {
    if (args.size() < 5) throw std::runtime_error("draw_line() requires start_x, start_y, end_x, end_y, and color");

    DrawLine(
        (int)args[0].asInt(),
        (int)args[1].asInt(),
        (int)args[2].asInt(),
        (int)args[3].asInt(),
        GetColor((uint32_t)args[4].asInt())
    );
    return Value();
}

Value native_draw_rect(std::vector<Value>& args) {
    if (args.size() < 5) throw std::runtime_error("draw_rect() requires x, y, w, h, and color");
    
    DrawRectangle(
        (int)args[0].asInt(),
        (int)args[1].asInt(),
        (int)args[2].asInt(),
        (int)args[3].asInt(),
        GetColor((uint32_t)args[4].asInt())
    );
    return Value();
}

Value native_draw_circle(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("draw_circle() requires x, y, r, and color");
    
    DrawCircle(
        (int)args[0].asInt(),
        (int)args[1].asInt(),
        (float)args[2].asFloat(),
        GetColor((uint32_t)args[3].asInt())
    );
    return Value();
}

Value native_draw_texture(std::vector<Value>& args) {
    if (args.size() < 5) throw std::runtime_error("draw_texture() requires tex_ptr, x, y, w, h");

    Texture2D* tex = reinterpret_cast<Texture2D*>(args[0].asInt());
    float x = (float)args[1].asFloat();
    float y = (float)args[2].asFloat();
    float w = (float)args[3].asFloat();
    float h = (float)args[4].asFloat();
    Color color = (args.size() > 5) ? GetColor((uint32_t)args[5].asInt()) : WHITE;

    if (tex) {
        Rectangle source = { 0.0f, 0.0f, (float)tex->width, (float)tex->height };
        Rectangle dest = { x, y, w, h };
        Vector2 origin = { 0.0f, 0.0f };

        DrawTexturePro(*tex, source, dest, origin, 0.0f, color);
    }
    return Value();
}

Value native_rgba(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("rgba() requires 4 arguments: r, g, b, a");

    uint32_t r = static_cast<uint32_t>(args[0].asInt()) & 0xFF;
    uint32_t g = static_cast<uint32_t>(args[1].asInt()) & 0xFF;
    uint32_t b = static_cast<uint32_t>(args[2].asInt()) & 0xFF;
    uint32_t a = static_cast<uint32_t>(args[3].asInt()) & 0xFF;

    uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | a;

    return Value(static_cast<int64_t>(rgba));
}

Value native_load_font(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("load_font() requires path");
    std::string path = args[0].asString();
    
    Font* font = new Font(LoadFont(path.c_str()));
    return Value(reinterpret_cast<int64_t>(font));
}

Value native_draw_text(std::vector<Value>& args) {
    if (args.size() < 5) throw std::runtime_error("draw_text() requires text, x, y, size, color");
    
    std::string text = args[0].toString();
    int x = (int)args[1].asInt();
    int y = (int)args[2].asInt();
    int fontSize = (int)args[3].asInt();
    Color color = GetColor((uint32_t)args[4].asInt());

    DrawText(text.c_str(), x, y, fontSize, color);
    return Value();
}

Value native_draw_text_ex(std::vector<Value>& args) {
    if (args.size() < 6) throw std::runtime_error("draw_text_ex() requires font, text, x, y, size, color");

    Font* font = reinterpret_cast<Font*>(args[0].asInt());
    std::string text = args[1].asString();
    Vector2 pos = { (float)args[2].asFloat(), (float)args[3].asFloat() };
    float fontSize = (float)args[4].asFloat();
    Color color = GetColor((uint32_t)args[5].asInt());

    DrawTextEx(*font, text.c_str(), pos, fontSize, 2.0f, color);
    return Value();
}

Value native_measure_text_ex(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("measure_text() requires font, text, size");
    Font* font = reinterpret_cast<Font*>(args[0].asInt());
    std::string text = args[1].asString();
    float fontSize = (float)args[2].asFloat();
    Vector2 size = MeasureTextEx(*font, text.c_str(), fontSize, 2.0f);
    return Value(std::vector<Value>{ Value(size.x), Value(size.y) });
}

Value native_draw_phase_scope(std::vector<Value>& args) {
    if (args.size() < 8) {
        throw std::runtime_error("draw_phase_scope() requires x_start, x_end, y_center, phase_a, phase_b, intensity, play_a, play_b");
    }

    float x_start   = static_cast<float>(args[0].asFloat());
    float x_end     = static_cast<float>(args[1].asFloat());
    float y_center  = static_cast<float>(args[2].asFloat());
    float phase_a   = static_cast<float>(args[3].asFloat());
    float phase_b   = static_cast<float>(args[4].asFloat());
    float intensity = static_cast<float>(args[5].asFloat());
    bool  play_a    = args[6].isTruthy();
    bool  play_b    = args[7].isTruthy();

    float wave_freq  = 15.0f + (intensity * 20.0f);
    float wave_amp   = 8.0f  + (intensity * 30.0f);
    float z_step     = 6.0f;

    float prev_zx   = x_start;
    float prev_zy_a = y_center;
    float prev_zy_b = y_center;

    Color col_deck_a = { 0, 240, 255, 255 };
    Color col_deck_b = { 255, 45, 120, 255 };
    Color glow_a     = { 0, 180, 220, 80 };
    Color glow_b     = { 200, 30, 90, 80 };

    float total_width = x_end - x_start;

    for (float curr_zx = x_start; curr_zx <= x_end; curr_zx += z_step) {
        float norm_z = (curr_zx - x_start) / total_width;

        float za = std::sin(norm_z * wave_freq + phase_a) * wave_amp;
        float zb = std::sin(norm_z * (wave_freq * 0.98f) + phase_b) * wave_amp;

        if (intensity > 0.8f) {
            za += std::sin(norm_z * wave_freq * 1.5f + phase_a) * wave_amp * 0.12f;
            zb += std::cos(norm_z * wave_freq * 1.5f + phase_b) * wave_amp * 0.12f;
        }

        float curr_zy_a = y_center - za;
        float curr_zy_b = y_center - zb;

        if (curr_zx > x_start) {
            if (play_a || (!play_a && !play_b)) {
                DrawLineV({ prev_zx, prev_zy_a - 1.0f }, { curr_zx, curr_zy_a - 1.0f }, glow_a);
                DrawLineV({ prev_zx, prev_zy_a + 1.0f }, { curr_zx, curr_zy_a + 1.0f }, glow_a);
                DrawLineV({ prev_zx, prev_zy_a }, { curr_zx, curr_zy_a }, col_deck_a);
            }
            if (play_b) {
                DrawLineV({ prev_zx, prev_zy_b - 1.0f }, { curr_zx, curr_zy_b - 1.0f }, glow_b);
                DrawLineV({ prev_zx, prev_zy_b + 1.0f }, { curr_zx, curr_zy_b + 1.0f }, glow_b);
                DrawLineV({ prev_zx, prev_zy_b }, { curr_zx, curr_zy_b }, col_deck_b);
            }
        }

        prev_zx = curr_zx;
        prev_zy_a = curr_zy_a;
        prev_zy_b = curr_zy_b;
    }

    return Value();
}

Value native_draw_spectrum_analyzer(std::vector<Value>& args) {
    if (args.size() < 6) {
        throw std::runtime_error("draw_spectrum_analyzer() requires x, y, width, height, num_bars, run_time, [intensity]");
    }

    float x         = static_cast<float>(args[0].asFloat());
    float y         = static_cast<float>(args[1].asFloat());
    float w         = static_cast<float>(args[2].asFloat());
    float h         = static_cast<float>(args[3].asFloat());
    int   num_bars  = static_cast<int>(args[4].asInt());
    float t         = static_cast<float>(args[5].asFloat());
    float intensity = (args.size() > 6) ? static_cast<float>(args[6].asFloat()) : 0.8f;

    float gap = 2.0f;
    float bar_w = (w - (num_bars - 1) * gap) / num_bars;

    Color col_a   = { 0, 240, 255, 200 };   // Electric Cyan
    Color col_b   = { 255, 45, 120, 200 };  // Neon Pink
    Color col_top = { 255, 170, 0, 255 };   // Amber Peak Cap

    for (int i = 0; i < num_bars; i++) {
        float norm_i = (float)i / (float)num_bars;

        float freq_weight = std::pow(1.0f - norm_i * 0.55f, 1.2f);

        float n1 = std::sin(t * 14.0f + i * 0.85f);
        float n2 = std::cos(t * 22.0f - i * 1.45f);
        float n3 = std::sin(t * 31.0f + i * 2.20f);

        float raw_val = std::abs(n1 * 0.5f + n2 * 0.3f + n3 * 0.2f);
        float bar_h   = std::clamp((raw_val * freq_weight * intensity) * h, 2.0f, h);

        float bx = x + i * (bar_w + gap);
        float by = y + h - bar_h;

        Color bar_col = {
            static_cast<unsigned char>(col_a.r * (1.0f - norm_i) + col_b.r * norm_i),
            static_cast<unsigned char>(col_a.g * (1.0f - norm_i) + col_b.g * norm_i),
            static_cast<unsigned char>(col_a.b * (1.0f - norm_i) + col_b.b * norm_i),
            180
        };

        DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bar_w), static_cast<int>(bar_h), bar_col);

        DrawRectangle(static_cast<int>(bx), static_cast<int>(by - 2.0f), static_cast<int>(bar_w), 2, col_top);
    }

    return Value();
}

} // namespace VGLibNative