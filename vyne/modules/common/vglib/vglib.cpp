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

        unsigned int flags = FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI;

        if (args.size() >= 4) {
            flags |= (unsigned int)args[3].asInt();
        }

        SetConfigFlags(flags);
        InitWindow(w, h, title.c_str());
        SetTargetFPS(60);
        
        return Value(true);
    }

    Value native_update_camera(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("update_camera() requires camera_ptr and mode");
        
        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        int mode = (int)args[1].asInt();

        if (camera) {
            UpdateCamera(camera, mode);
        }
        return Value();
    }

    Value native_is_running(std::vector<Value>& args) {
        return Value(!WindowShouldClose());
    }

    Value native_begin_frame(std::vector<Value>& args) {
        BeginDrawing();
        rlClearScreenBuffers();
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
            
            rlEnableDepthTest(); 
            
            DrawCube({0, 0, 0}, size, size, size, color);
            DrawCubeWires({0, 0, 0}, size, size, size, Fade(BLACK, 0.5f));
            
            rlDisableDepthTest();
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

        return Value(reinterpret_cast<int64_t>(&camera));
    }

    Value native_begin_3d_mode(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("begin_3d_mode() requires a camera pointer");

        int64_t ptr_val = args[0].asInt();
        Camera3D* camera = reinterpret_cast<Camera3D*>(ptr_val);
        
        // hələlik sabit qalsın deyə bunu rəyə alırıq
        // UpdateCamera(camera, CAMERA_ORBITAL); 
        
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

    Value native_is_key_pressed(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        int key = (int)args[0].asInt();
        return Value(IsKeyPressed(key));
    }

    Value native_is_mouse_button_down(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        int button = (int)args[0].asInt();
        return Value(IsMouseButtonDown(button));
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

    Value native_draw_plane(std::vector<Value>& args) {
        Vector3 pos = { (float)args[0].asFloat(), (float)args[1].asFloat(), (float)args[2].asFloat() };
        Vector2 size = { (float)args[3].asFloat(), (float)args[4].asFloat() };
        Color color = GetColor((uint32_t)args[5].asInt());
        
        DrawPlane(pos, size, color);
        return Value();
    }

    Value native_clear_gradient(std::vector<Value>& args) {
        Color top = GetColor((uint32_t)args[0].asInt());
        Color bottom = GetColor((uint32_t)args[1].asInt());
        DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), top, bottom);
        return Value();
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

    Value native_draw_grid(std::vector<Value>& args) {
        int slices = (args.size() > 0) ? (int)args[0].asInt() : 10;
        float spacing = (args.size() > 1) ? (float)args[1].asFloat() : 1.0f;
        
        DrawGrid(slices, spacing);
        return Value();
    }

    Value native_init_audio(std::vector<Value>& args) {
        InitAudioDevice();
        return Value(IsAudioDeviceReady());
    }

    Value native_load_sound(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("load_sound() requires a file path");
        std::string path = args[0].asString();
        
        Sound* sound = new Sound(LoadSound(path.c_str()));
        
        if (sound->frameCount == 0) {
            delete sound;
            throw std::runtime_error("Audio Error: Could not load sound at " + path);
        }
        
        return Value(reinterpret_cast<int64_t>(sound));
    }

    Value native_play_sound(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        if (sound) {
            PlaySound(*sound);
            return Value(true);
        }
        return Value(false);
    }

    Value native_set_master_volume(std::vector<Value>& args) {
        if (args.empty()) return Value(false);
        float volume = (float)args[0].asFloat();
        SetMasterVolume(volume);
        return Value(true);
    }

    Value native_set_sound_volume(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("set_sound_volume() requires sound_pointer and volume");
        
        Sound* sound = reinterpret_cast<Sound*>(args[0].asInt());
        float volume = (float)args[1].asFloat();
        
        if (sound) {
            SetSoundVolume(*sound, volume);
            return Value(true);
        }
        return Value(false);
    }

    Value native_close_audio(std::vector<Value>& args) {
        CloseAudioDevice();
        return Value();
    }

    Value native_disable_cursor(std::vector<Value>& args) {
        DisableCursor();
        return Value();
    }

    Value native_enable_cursor(std::vector<Value>& args) {
        EnableCursor();
        return Value();
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
    vglib[pool.intern("key_pressed")] = Value(VGLibNative::native_is_key_pressed);
    vglib[pool.intern("mouse_down")]  = Value(VGLibNative::native_is_mouse_button_down);
    vglib[pool.intern("rgba")]        = Value(VGLibNative::native_rgba);
    vglib[pool.intern("plane")]       = Value(VGLibNative::native_draw_plane);
    vglib[pool.intern("clear_gradient")]   = Value(VGLibNative::native_clear_gradient);
    vglib[pool.intern("text")]        = Value(VGLibNative::native_draw_text);
    vglib[pool.intern("grid")]        = Value(VGLibNative::native_draw_grid);
    vglib[pool.intern("init_audio")]  = Value(VGLibNative::native_init_audio);
    vglib[pool.intern("load_sound")]  = Value(VGLibNative::native_load_sound);
    vglib[pool.intern("play_sound")]  = Value(VGLibNative::native_play_sound);
    vglib[pool.intern("close_audio")] = Value(VGLibNative::native_close_audio);
    vglib[pool.intern("volume")]      = Value(VGLibNative::native_set_master_volume);
    vglib[pool.intern("sound_volume")] = Value(VGLibNative::native_set_sound_volume);
    vglib[pool.intern("update_camera")] = Value(VGLibNative::native_update_camera);
    vglib[pool.intern("disable_cursor")] = Value(VGLibNative::native_disable_cursor);
    vglib[pool.intern("enable_cursor")]  = Value(VGLibNative::native_enable_cursor);

    // VGLib properties
    vglib[pool.intern("version")]  = Value("v0.0.1-alpha").setReadOnly();

    // keyboard codes important
    vglib[pool.intern("SPACE")]  = Value(32).setReadOnly();
    vglib[pool.intern("ENTER")]  = Value(257).setReadOnly();
    vglib[pool.intern("ESCAPE")] = Value(256).setReadOnly();
    
    // wasd
    vglib[pool.intern("W")] = Value(87).setReadOnly();
    vglib[pool.intern("A")] = Value(65).setReadOnly();
    vglib[pool.intern("S")] = Value(83).setReadOnly();
    vglib[pool.intern("D")] = Value(68).setReadOnly();

    // mouse btns
    vglib[pool.intern("MOUSE_LEFT")]  = Value(0).setReadOnly();
    vglib[pool.intern("MOUSE_RIGHT")] = Value(1).setReadOnly();

    // arrows
    vglib[pool.intern("UP")]    = Value(265).setReadOnly();
    vglib[pool.intern("DOWN")]  = Value(264).setReadOnly();
    vglib[pool.intern("LEFT")]  = Value(263).setReadOnly();
    vglib[pool.intern("RIGHT")] = Value(262).setReadOnly();

    // preset colors
    vglib[pool.intern("WHITE")]   = Value((int64_t)0xFFFFFFFF).setReadOnly();
    vglib[pool.intern("BLACK")]   = Value((int64_t)0x000000FF).setReadOnly();
    vglib[pool.intern("RED")]     = Value((int64_t)0xFF0000FF).setReadOnly();
    vglib[pool.intern("GREEN")]   = Value((int64_t)0x00FF00FF).setReadOnly();
    vglib[pool.intern("BLUE")]    = Value((int64_t)0x0000FFFF).setReadOnly();
    vglib[pool.intern("CYAN")]    = Value((int64_t)0x00AAFFFF).setReadOnly();
    vglib[pool.intern("MAGENTA")] = Value((int64_t)0xFF00FFFF).setReadOnly();
    vglib[pool.intern("PURPLE")]  = Value((int64_t)0x800080FF).setReadOnly();

    // video props
    vglib[pool.intern("FULLSCREEN")]      = Value(2).setReadOnly(); // FLAG_FULLSCREEN_WINDOW = 0x00000002
    vglib[pool.intern("VSYNC")]           = Value(64).setReadOnly(); // FLAG_VSYNC_HINT = 0x00000040
    vglib[pool.intern("CAMERA_FREE")]     = Value(0).setReadOnly();
    vglib[pool.intern("CAMERA_ORBITAL")]  = Value(2).setReadOnly();
    vglib[pool.intern("CAMERA_FIRST_P")]  = Value(3).setReadOnly();
    vglib[pool.intern("CAMERA_THIRD_P")]  = Value(4).setReadOnly();
}