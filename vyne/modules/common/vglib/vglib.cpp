#include "vglib.h"
#include <cstring>

/**
 * VGLib Native Method Implementations
 */

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
        if (args.size() < 3) throw std::runtime_error("init() requires width, height, and title");
        
        int w = (int)args[0].asInt();
        int h = (int)args[1].asInt();
        std::string title = args[2].asString();

        InitWindow(w, h, title.c_str());
        SetTargetFPS(60); // standart 60 FPS, gelecekde argument kimi pass edib deyiserik
        return Value(true);
    }

    Value native_is_running(std::vector<Value>& args) {
        return Value(!WindowShouldClose());
    }

    Value native_begin_frame(std::vector<Value>& args) {
        BeginDrawing();
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

    Value native_draw_cube(std::vector<Value>& args) {
        if (args.size() < 6) throw std::runtime_error("draw_cube() requires x, y, z, size, rotation, and color");
        
        float x = (float)args[0].asFloat();
        float y = (float)args[1].asFloat();
        float z = (float)args[2].asFloat();
        float size = (float)args[3].asFloat();
        float rotAngle = (float)args[4].asFloat();
        Color color = GetColor((uint32_t)args[5].asInt());

        rlPushMatrix();
            rlTranslatef(x, y, z);
            rlRotatef(rotAngle, 1.0f, 1.0f, 1.0f);
            
            DrawCube({0, 0, 0}, size, size, size, color);
            DrawCubeWires({0, 0, 0}, size, size, size, BLACK);
        rlPopMatrix();

        return Value();
    }

    Value native_init_3d_camera(std::vector<Value>& args) {
        static Camera3D camera = { 0 };
        camera.position = (Vector3){ 10.0f, 10.0f, 10.0f };
        camera.target = (Vector3){ 0.0f, 0.0f, 0.0f }; 
        camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        UpdateCamera(&camera, CAMERA_ORBITAL);
        return Value(reinterpret_cast<int64_t>(&camera));
    }

    Value native_begin_3d_mode(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("begin_3d_mode() requires a camera pointer");
        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        UpdateCamera(camera, CAMERA_ORBITAL); // Kameranı fırlada bilək (orbit mode)
        BeginMode3D(*camera);
        return Value();
    }

    Value native_end_3d_mode(std::vector<Value>& args) {
        EndMode3D();
        return Value();
    }

    Value native_close(std::vector<Value>& args) {
        CloseWindow();
        return Value();
    }

    Value native_is_key_down(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        int key = (int)args[0].asInt();
        return Value(IsKeyDown(key));
    }
}

void setupVGLib(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "vglib";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vglib = env[path];

    // VGLib methods
    vglib[pool.intern("donut")]      = Value(VGLibNative::native_donut);
    vglib[pool.intern("init")]       = Value(VGLibNative::native_init);
    vglib[pool.intern("running")]    = Value(VGLibNative::native_is_running);
    vglib[pool.intern("begin")]      = Value(VGLibNative::native_begin_frame);
    vglib[pool.intern("end")]        = Value(VGLibNative::native_end_frame);
    vglib[pool.intern("clear")]      = Value(VGLibNative::native_clear);
    vglib[pool.intern("rect")]       = Value(VGLibNative::native_draw_rect);
    vglib[pool.intern("circle")]     = Value(VGLibNative::native_draw_circle);
    vglib[pool.intern("close")]      = Value(VGLibNative::native_close);
    vglib[pool.intern("cube")]       = Value(VGLibNative::native_draw_cube);
    vglib[pool.intern("camera")]     = Value(VGLibNative::native_init_3d_camera);
    vglib[pool.intern("begin3d")]    = Value(VGLibNative::native_begin_3d_mode);
    vglib[pool.intern("end3d")]      = Value(VGLibNative::native_end_3d_mode);
    vglib[pool.intern("key_down")]   = Value(VGLibNative::native_is_key_down);

    // VGLib properties
    vglib[pool.intern("version")]  = Value("v0.0.1-alpha").setReadOnly();
}