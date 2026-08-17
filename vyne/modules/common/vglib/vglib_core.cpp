#include "vglib_common.h"

namespace VGLibNative {

Value native_donut(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("donut() requires A and B arguments");

    std::printf("\x1b[H\x1b[?25l\x1b[J");

    float A = static_cast<float>(args[0].asFloat());
    float B = static_cast<float>(args[1].asFloat());

    float z[1760];
    char b[1760];
    std::memset(b, 32, 1760);
    std::memset(z, 0, sizeof(z));

    float sinA = std::sin(A), cosA = std::cos(A);
    float sinB = std::sin(B), cosB = std::cos(B);

    for (float j = 0; j < 6.28f; j += 0.07f) {
        float ct = std::cos(j), st = std::sin(j);
        for (float i = 0; i < 6.28f; i += 0.02f) {
            float sp = std::sin(i), cp = std::cos(i);
            float h = ct + 2;
            float D = 1 / (sp * h * sinA + st * cosA + 5);
            float t = sp * h * cosA - st * sinA;

            int x = static_cast<int>(40 + 30 * D * (cp * h * cosB - t * sinB));
            int y = static_cast<int>(12 + 15 * D * (cp * h * sinB + t * cosB));
            int o = x + 80 * y;
            int N = static_cast<int>(8 * ((st * sinA - sp * ct * cosA) * cosB - sp * ct * sinA - st * cosA - cp * ct * sinB));

            if (22 > y && y > 0 && x > 0 && 80 > x && D > z[o]) {
                z[o] = D;
                b[o] = ".,-~:;=!*#$@"[N > 0 ? (N < 12 ? N : 11) : 0];
            }
        }
    }

    char output[2000];
    int p = 0;
    for (int j = 0; j < 22; j++) {
        for (int i = 0; i < 80; i++) {
            output[p++] = b[i + j * 80];
        }
        output[p++] = '\n';
    }
    std::fwrite(output, 1, p, stdout);
    std::fflush(stdout);

    return Value();
}

Value native_init(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("init() requires width, height, FPS, title");
    
    int w = (int)args[0].asInt();
    int h = (int)args[1].asInt();
    int fps = (int)args[2].asInt();
    const std::string& title = args[3].asString();

    unsigned int flags = FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_UNDECORATED;

    if (args.size() >= 5) {
        flags |= (unsigned int)args[4].asInt();
    }

    if (flags & 2) {
        flags |= FLAG_WINDOW_UNDECORATED; 
    }

    SetConfigFlags(flags);
    InitWindow(w, h, title.c_str());
    SetTargetFPS(fps);
    
    if (IsWindowFullscreen() || (flags & 2)) {
        int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
    }
    
    return Value(true);
}

Value native_is_running(std::vector<Value>& args) {
    return Value(!WindowShouldClose());
}

Value native_begin_frame(std::vector<Value>& args) {
    BeginDrawing();
    rlClearScreenBuffers();
    BeginBlendMode(BLEND_ALPHA);
    return Value();
}

Value native_end_frame(std::vector<Value>& args) {
    EndDrawing();
    return Value();
}

Value native_clear(std::vector<Value>& args) {
    uint32_t color = (args.empty()) ? 0x000000FF : (uint32_t)args[0].asInt();
    ClearBackground(GetColor(color));
    return Value();
}

Value native_clear_gradient(std::vector<Value>& args) {
    Color top = GetColor((uint32_t)args[0].asInt());
    Color bottom = GetColor((uint32_t)args[1].asInt());
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), top, bottom);
    return Value();
}

Value native_close(std::vector<Value>& args) {
    CloseWindow();
    return Value();
}

Value native_get_fps(std::vector<Value>& args) {
    return Value((int64_t)GetFPS());
}

} // namespace VGLibNative