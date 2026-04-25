#include "vglib.h"
#include <cstring>
#include <map>

// helpers

struct PersistentInstance {
    Vector3 position;
    float scale;
};

static std::map<std::string, std::vector<PersistentInstance>> persistent_groups;

static inline void DrawFace(
    Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4,
    Vector3 normal
) {
    rlNormal3f(normal.x, normal.y, normal.z);

    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(v1.x, v1.y, v1.z);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(v2.x, v2.y, v2.z);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(v3.x, v3.y, v3.z);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(v4.x, v4.y, v4.z);
}

void DrawCubeTexture(Texture2D texture, Vector3 position,
                     float width, float height, float length,
                     Color color)
{
    float hw = width  * 0.5f;
    float hh = height * 0.5f;
    float hl = length * 0.5f;

    float x = position.x;
    float y = position.y;
    float z = position.z;

    // precompute vertices
    Vector3 p000 = { x - hw, y - hh, z - hl };
    Vector3 p001 = { x - hw, y - hh, z + hl };
    Vector3 p010 = { x - hw, y + hh, z - hl };
    Vector3 p011 = { x - hw, y + hh, z + hl };
    Vector3 p100 = { x + hw, y - hh, z - hl };
    Vector3 p101 = { x + hw, y - hh, z + hl };
    Vector3 p110 = { x + hw, y + hh, z - hl };
    Vector3 p111 = { x + hw, y + hh, z + hl };

    rlCheckRenderBatchLimit(36);
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);

    rlColor4ub(color.r, color.g, color.b, color.a);

    // Front (+Z)
    DrawFace(p001, p101, p111, p011, { 0, 0, 1 });

    // Back (-Z)
    DrawFace(p100, p000, p010, p110, { 0, 0, -1 });

    // Top (+Y)
    DrawFace(p011, p111, p110, p010, { 0, 1, 0 });

    // Bottom (-Y)
    DrawFace(p000, p100, p101, p001, { 0, -1, 0 });

    // Right (+X)
    DrawFace(p101, p100, p110, p111, { 1, 0, 0 });

    // Left (-X)
    DrawFace(p000, p001, p011, p010, { -1, 0, 0 });

    rlEnd();
    rlSetTexture(0);
}

void DrawPlaneTexture(Texture2D texture, Vector3 centerPos, Vector2 size, Color color) {
    float tileX = size.x / 2.0f; 
    float tileY = size.y / 2.0f;

    rlCheckRenderBatchLimit(4);
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlNormal3f(0.0f, 1.0f, 0.0f);

        rlTexCoord2f(0.0f, 0.0f); rlVertex3f(centerPos.x - size.x/2, centerPos.y, centerPos.z - size.y/2);
        rlTexCoord2f(0.0f, tileY); rlVertex3f(centerPos.x - size.x/2, centerPos.y, centerPos.z + size.y/2);
        rlTexCoord2f(tileX, tileY); rlVertex3f(centerPos.x + size.x/2, centerPos.y, centerPos.z + size.y/2);
        rlTexCoord2f(tileX, 0.0f); rlVertex3f(centerPos.x + size.x/2, centerPos.y, centerPos.z - size.y/2);
    rlEnd();
    rlSetTexture(0);
}

void DrawBillboardVyne(Camera3D camera, Texture2D texture, Vector3 position, float size, Color color) {
    DrawBillboard(camera, texture, position, size, color);
}

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
        if (args.size() < 4) throw std::runtime_error("init() requires width, height, FPS, title");
        
        int w = (int)args[0].asInt();
        int h = (int)args[1].asInt();
        int fps = (int)args[2].asInt();
        std::string title = args[3].asString();

        unsigned int flags = FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI;

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

    Value native_draw_line(std::vector<Value>& args) {
        if (args.size() < 5) throw std::runtime_error("draw_line() requires start_x, start_y, end_x, end_y, and color");

        DrawLine(
            (int)args[0].asInt(),                   // startPosX
            (int)args[1].asInt(),                   // startPosY
            (int)args[2].asInt(),                   // endPosX
            (int)args[3].asInt(),                   // endPosY
            GetColor((uint32_t)args[4].asInt())     // color
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
        
        if (!args.empty()) {
            camera.fovy = (float)args[0].asFloat();
        } else {
            camera.fovy = 45.0f;
        }
        
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

    Value native_load_font(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("load_font() requires path");
        std::string path = args[0].asString();
        
        Font* font = new Font(LoadFont(path.c_str()));
        return Value(reinterpret_cast<int64_t>(font));
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

    

    Value native_disable_cursor(std::vector<Value>& args) {
        DisableCursor();
        return Value();
    }

    Value native_enable_cursor(std::vector<Value>& args) {
        EnableCursor();
        return Value();
    }

    Value native_get_fps(std::vector<Value>& args) {
        return Value((int64_t)GetFPS());
    }

    Value native_set_camera_height(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("set_camera_height() requires camera_ptr and height");

        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        float newHeight = (float)args[1].asFloat();

        if (camera) {
            float diffY = camera->target.y - camera->position.y;
            camera->position.y = newHeight;
            camera->target.y = camera->position.y + diffY;
        }
        return Value();
    }

    Value native_move_forward(std::vector<Value>& args) {
        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        float distance = (float)args[1].asFloat();

        if (camera) {
            Vector3 forward = {
                camera->target.x - camera->position.x,
                0,
                camera->target.z - camera->position.z
            };
            
            float len = sqrt(forward.x * forward.x + forward.z * forward.z);
            if (len > 0) { forward.x /= len; forward.z /= len; }

            camera->position.x += forward.x * distance;
            camera->position.z += forward.z * distance;
            camera->target.x += forward.x * distance;
            camera->target.z += forward.z * distance;
        }
        return Value();
    }

    Value native_move_right(std::vector<Value>& args) {
        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        float distance = (float)args[1].asFloat();

        if (camera) {
            Vector3 forward = { camera->target.x - camera->position.x, 0, camera->target.z - camera->position.z };
            Vector3 right = { -forward.z, 0, forward.x }; 
            
            float len = sqrt(right.x * right.x + right.z * right.z);
            if (len > 0) { right.x /= len; right.z /= len; }

            camera->position.x += right.x * distance;
            camera->position.z += right.z * distance;
            camera->target.x += right.x * distance;
            camera->target.z += right.z * distance;
        }
        return Value();
    }

    Value native_rotate_view(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("rotate_view() requires camera_ptr and sensitivity");

        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        float sensitivity = (float)args[1].asFloat();

        if (camera) {
            Vector2 mouseDelta = GetMouseDelta();
            
            UpdateCameraPro(camera, 
                (Vector3){ 0, 0, 0 }, 
                (Vector3){ mouseDelta.x * sensitivity, mouseDelta.y * sensitivity, 0 }, 
                0.0f
            );
        }
        return Value();
    }

    Value native_load_shader(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("load_shader() requires at least one path");

        if (args.size() == 2) {
            std::string vsPath = args[0].asString();
            std::string fsPath = args[1].asString();
            Shader* shader = new Shader(LoadShader(vsPath.c_str(), fsPath.c_str()));
            return Value(reinterpret_cast<int64_t>(shader));
        } else {
            std::string fsPath = args[0].asString();
            Shader* shader = new Shader(LoadShader(0, fsPath.c_str()));
            return Value(reinterpret_cast<int64_t>(shader));
        }
    }

    Value native_begin_shader(std::vector<Value>& args) {
        Shader* shader = reinterpret_cast<Shader*>(args[0].asInt());
        BeginShaderMode(*shader);
        return Value();
    }

    Value native_end_shader(std::vector<Value>& args) {
        EndShaderMode();
        return Value();
    }

    Value native_set_shader_camera(std::vector<Value>& args) {
        Shader* shader = reinterpret_cast<Shader*>(args[0].asInt());
        Camera3D* camera = reinterpret_cast<Camera3D*>(args[1].asInt());
        
        int cameraPosLoc = GetShaderLocation(*shader, "cameraPos");
        float cameraPos[3] = { camera->position.x, camera->position.y, camera->position.z };
        SetShaderValue(*shader, cameraPosLoc, cameraPos, SHADER_UNIFORM_VEC3);
        return Value();
    }

    Value native_get_camera_pos(std::vector<Value>& args) {
        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        std::vector<Value> pos = { Value(camera->position.x), Value(camera->position.y), Value(camera->position.z) };
        return Value(pos);
    }

    Value native_set_camera_pos(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("set_pos() requires camera_ptr, x, y, z");

        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        float x = (float)args[1].asFloat();
        float y = (float)args[2].asFloat();
        float z = (float)args[3].asFloat();

        if (camera) {
            Vector3 direction = {
                camera->target.x - camera->position.x,
                camera->target.y - camera->position.y,
                camera->target.z - camera->position.z
            };

            camera->position = (Vector3){ x, y, z };

            camera->target = (Vector3){ x + direction.x, y + direction.y, z + direction.z };
        }
        return Value();
    }

    Value native_check_collision(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("check_collision() requires player_pos, player_size, cube_pos, cube_size");

        Vector3 pPos = { (float)args[0].asList()[0].asFloat(), (float)args[0].asList()[1].asFloat(), (float)args[0].asList()[2].asFloat() };
        Vector3 pSize = { (float)args[1].asList()[0].asFloat(), (float)args[1].asList()[1].asFloat(), (float)args[1].asList()[2].asFloat() };

        Vector3 cPos = { (float)args[2].asList()[0].asFloat(), (float)args[2].asList()[1].asFloat(), (float)args[2].asList()[2].asFloat() };
        float cSize = (float)args[3].asFloat();

        BoundingBox playerBox = { 
            (Vector3){ pPos.x - pSize.x/2, pPos.y - pSize.y/2 + 0.2f, pPos.z - pSize.z/2 },
            (Vector3){ pPos.x + pSize.x/2, pPos.y + pSize.y/2, pPos.z + pSize.z/2 }
        };

        BoundingBox cubeBox = {
            (Vector3){ cPos.x - cSize/2, cPos.y - cSize/2, cPos.z - cSize/2 },
            (Vector3){ cPos.x + cSize/2, cPos.y + cSize/2, cPos.z + cSize/2 }
        };

        return Value(CheckCollisionBoxes(playerBox, cubeBox));
    }

    Value native_load_render_texture(std::vector<Value>& args) {
        int w = (int)args[0].asInt();
        int h = (int)args[1].asInt();
        RenderTexture2D* target = new RenderTexture2D(LoadRenderTexture(w, h));
        return Value(reinterpret_cast<int64_t>(target));
    }

    Value native_begin_texture_mode(std::vector<Value>& args) {
        RenderTexture2D* target = reinterpret_cast<RenderTexture2D*>(args[0].asInt());
        BeginTextureMode(*target);
        return Value();
    }

    Value native_end_texture_mode(std::vector<Value>& args) {
        EndTextureMode();
        return Value();
    }

    Value native_draw_render_texture(std::vector<Value>& args) {
        RenderTexture2D* target = reinterpret_cast<RenderTexture2D*>(args[0].asInt());
        DrawTextureRec(target->texture, (Rectangle){ 0, 0, (float)target->texture.width, (float)-target->texture.height }, (Vector2){ 0, 0 }, WHITE);
        return Value();
    }

    Value native_set_shader_value(std::vector<Value>& args) {
        if (args.size() < 3) throw std::runtime_error("set_shader_value() requires 3 args");

        Shader* shader = reinterpret_cast<Shader*>(args[0].asInt());
        std::string uniformName = args[1].asString();
        int loc = GetShaderLocation(*shader, uniformName.c_str());

        if (args[2].type == VType::Array) {
            std::vector<Value> list = args[2].asList();
            float values[2] = { (float)list[0].asFloat(), (float)list[1].asFloat() };
            SetShaderValue(*shader, loc, values, SHADER_UNIFORM_VEC2);
        } else {
            float value = (float)args[2].asFloat();
            SetShaderValue(*shader, loc, &value, SHADER_UNIFORM_FLOAT);
        }
        return Value();
    }

    Value native_load_texture(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("load_texture() requires a file path");
        std::string path = args[0].asString();
        
        Texture2D* tex = new Texture2D(LoadTexture(path.c_str()));
        return Value(reinterpret_cast<int64_t>(tex));
    }

    Value native_draw_cube_texture(std::vector<Value>& args) {
        if (args.size() < 5) throw std::runtime_error("cube_texture() requires tex_ptr, x, y, z, size");

        Texture2D* tex = reinterpret_cast<Texture2D*>(args[0].asInt());
        Vector3 pos = { (float)args[1].asFloat(), (float)args[2].asFloat(), (float)args[3].asFloat() };
        float size = (float)args[4].asFloat();
        Color color = (args.size() > 5) ? GetColor((uint32_t)args[5].asInt()) : WHITE;

        DrawCubeTexture(*tex, pos, size, size, size, color);
        
        return Value();
    }

    Value native_draw_plane_texture(std::vector<Value>& args) {
        Texture2D* tex = reinterpret_cast<Texture2D*>(args[0].asInt());
        Vector3 pos = { (float)args[1].asFloat(), (float)args[2].asFloat(), (float)args[3].asFloat() };
        Vector2 size = { (float)args[4].asFloat(), (float)args[5].asFloat() };
        
        DrawPlaneTexture(*tex, pos, size, WHITE);
        return Value();
    }

    Value native_get_mouse_pos(std::vector<Value>& args) {
        Vector2 m = GetMousePosition();
        return Value(std::vector<Value>{ Value(m.x), Value(m.y) });
    }

    Value native_get_mouse_delta(std::vector<Value>& args) {
        Vector2 d = GetMouseDelta();
        return Value(std::vector<Value>{ Value(d.x), Value(d.y) });
    }

    Value native_draw_billboard(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("draw_billboard() requires camera_ptr, tex_ptr, pos_list, size");

        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        Texture2D* tex = reinterpret_cast<Texture2D*>(args[1].asInt());
        
        std::vector<Value> pList = args[2].asList();
        if (pList.size() < 3) throw std::runtime_error("billboard pos_list must have 3 elements [x,y,z]");

        Vector3 pos = { 
            (float)pList[0].asFloat(), 
            (float)pList[1].asFloat(), 
            (float)pList[2].asFloat() 
        };
        
        float size = (float)args[3].asFloat();
        Color color = (args.size() > 4) ? GetColor((uint32_t)args[4].asInt()) : WHITE;

        if (camera && tex) {
            DrawBillboard(*camera, *tex, pos, size, color);
        }
        return Value();
    }

    Value native_load_model(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("load_model() requires a file path");
        std::string path = args[0].asString();
        
        Model* model = new Model(LoadModel(path.c_str()));
        
        if (model->meshCount == 0) {
            delete model;
            throw std::runtime_error("Model Error: Could not load model from " + path);
        }
        
        return Value(reinterpret_cast<int64_t>(model));
    }

    Value native_set_model_texture(std::vector<Value>& args) {
        if (args.size() < 2) return Value(false);

        Model* model = reinterpret_cast<Model*>(args[0].asInt());
        Texture2D* tex = reinterpret_cast<Texture2D*>(args[1].asInt());

        if (model != nullptr && tex != nullptr) {
            if (model->materialCount > 0 && model->meshCount > 0) {
                model->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
                return Value(true);
            }
        }
        std::cout << "Vyne Warning: Model texture could not be set!" << std::endl;
        return Value(false);
    }

    Value native_draw_model(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("draw_model() requires model_ptr, x, y, z");

        Model* model = reinterpret_cast<Model*>(args[0].asInt());
        Vector3 pos = { (float)args[1].asFloat(), (float)args[2].asFloat(), (float)args[3].asFloat() };
        float scale = (args.size() > 4) ? (float)args[4].asFloat() : 1.0f;
        Color color = (args.size() > 5) ? GetColor((uint32_t)args[5].asInt()) : WHITE;

        if (model) {
            DrawModel(*model, pos, scale, color);
        }
        return Value();
    }

    Value native_load_map(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("load_map() requires a path");
        std::string path = args[0].asString();
        
        std::ifstream file(path);
        if (!file.is_open()) return Value(std::vector<Value>{}); // Fayl yoxdursa boş list qaytar

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        std::vector<Value> final_map;
        std::stringstream ss(content);
        std::string segment;

        while (std::getline(ss, segment, ';')) {
            if (segment.empty() || segment == "\n") continue;
            std::stringstream obj_ss(segment);
            std::string val;
            std::vector<Value> obj_data;
            
            while (std::getline(obj_ss, val, ',')) {
                if (!val.empty()) {
                    obj_data.emplace_back(static_cast<double>(std::atof(val.c_str())));
                }
            }
            
            if (obj_data.size() < 5) {
                obj_data.emplace_back(0.0); 
            }
            
            final_map.emplace_back(Value(obj_data));
        }
        
        return Value(final_map);
    }

    Value native_get_ray_grid(std::vector<Value>& args) {
        if (args.size() < 3) throw std::runtime_error("get_ray_grid() requires camera_ptr, distance, and grid_size");

        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        float distance = (float)args[1].asFloat();
        float grid_size = (float)args[2].asFloat();

        if (camera) {
            Vector3 forward = {
                camera->target.x - camera->position.x,
                camera->target.y - camera->position.y,
                camera->target.z - camera->position.z
            };

            float len = sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
            if (len > 0) {
                forward.x /= len; forward.y /= len; forward.z /= len;
            }

            Vector3 hitPoint = {
                camera->position.x + forward.x * distance,
                camera->position.y + forward.y * distance,
                camera->position.z + forward.z * distance
            };

            float snappedX = roundf(hitPoint.x / grid_size) * grid_size;
            float snappedY = roundf(hitPoint.y / grid_size) * grid_size;
            float snappedZ = roundf(hitPoint.z / grid_size) * grid_size;

            std::vector<Value> result = { Value(snappedX), Value(snappedY), Value(snappedZ) };
            return Value(result);
        }
        return Value(std::vector<Value>{Value(0.0), Value(0.0), Value(0.0)});
    }

    Value native_export_obj(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("export_obj() requires map_data and filename");
        
        std::vector<Value> map_data = args[0].asList();
        std::string filename = args[1].asString();
        
        std::ofstream file(filename, std::ios::out | std::ios::trunc);
        
        if (!file.is_open()) throw std::runtime_error("Could not create OBJ file");

        file << "# Vyne Static Map Export\n";
        file << "o MapMesh\n";

        // UV koordinatları
        file << "vt 0.0 0.0\n";
        file << "vt 1.0 0.0\n";
        file << "vt 1.0 1.0\n";
        file << "vt 0.0 1.0\n";

        int v_count = 1;

        for (const auto& obj_val : map_data) {
            std::vector<Value> obj = obj_val.asList();
            float x = (float)obj[0].asFloat();
            float y = (float)obj[1].asFloat();
            float z = (float)obj[2].asFloat();
            float s = (float)obj[3].asFloat() / 2.0f;

            file << "v " << x-s << " " << y-s << " " << z-s << "\n";
            file << "v " << x+s << " " << y-s << " " << z-s << "\n";
            file << "v " << x+s << " " << y+s << " " << z-s << "\n";
            file << "v " << x-s << " " << y+s << " " << z-s << "\n";
            file << "v " << x-s << " " << y-s << " " << z+s << "\n";
            file << "v " << x+s << " " << y-s << " " << z+s << "\n";
            file << "v " << x+s << " " << y+s << " " << z+s << "\n";
            file << "v " << x-s << " " << y+s << " " << z+s << "\n";

            int o = v_count;
            file << "f " << o << "/1 " << o+1 << "/2 " << o+2 << "/3 " << o+3 << "/4\n";
            file << "f " << o+4 << "/1 " << o+5 << "/2 " << o+6 << "/3 " << o+7 << "/4\n";
            file << "f " << o << "/1 " << o+1 << "/2 " << o+5 << "/3 " << o+4 << "/4\n";
            file << "f " << o+2 << "/1 " << o+3 << "/2 " << o+7 << "/3 " << o+6 << "/4\n";
            file << "f " << o << "/1 " << o+3 << "/2 " << o+7 << "/3 " << o+4 << "/4\n";
            file << "f " << o+1 << "/1 " << o+2 << "/2 " << o+6 << "/3 " << o+5 << "/4\n";

            v_count += 8;
        }

        file << "\n";
        file.close();
        return Value(true);
    }

    Value native_distance_3d(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("distance_3d() requires two position lists");
        
        std::vector<Value> a = args[0].asList();
        std::vector<Value> b = args[1].asList();
        
        float dx = (float)a[0].asFloat() - (float)b[0].asFloat();
        float dy = (float)a[1].asFloat() - (float)b[1].asFloat();
        float dz = (float)a[2].asFloat() - (float)b[2].asFloat();
        
        return Value((double)sqrtf(dx*dx + dy*dy + dz*dz));
    }

    Value native_check_collision_map(std::vector<Value>& args) {
        if (args.size() < 3) throw std::runtime_error("check_collision_map() requires player_pos, player_size, and map_data");

        std::vector<Value> pPosList = args[0].asList();
        std::vector<Value> pSizeList = args[1].asList();
        
        Vector3 pPos = { (float)pPosList[0].asFloat(), (float)pPosList[1].asFloat(), (float)pPosList[2].asFloat() };
        Vector3 pSize = { (float)pSizeList[0].asFloat(), (float)pSizeList[1].asFloat(), (float)pSizeList[2].asFloat() };

        BoundingBox playerBox = { 
            (Vector3){ pPos.x - pSize.x/2, pPos.y - pSize.y/2, pPos.z - pSize.z/2 },
            (Vector3){ pPos.x + pSize.x/2, pPos.y + pSize.y/2, pPos.z + pSize.z/2 }
        };

        std::vector<Value> mapData = args[2].asList();

        for (const auto& objVal : mapData) {
            std::vector<Value> obj = objVal.asList();
            
            float cX = (float)obj[0].asFloat();
            float cY = (float)obj[1].asFloat();
            float cZ = (float)obj[2].asFloat();
            float cSize = (float)obj[3].asFloat();

            BoundingBox cubeBox = {
                (Vector3){ cX - cSize/2, cY - cSize/2, cZ - cSize/2 },
                (Vector3){ cX + cSize/2, cY + cSize/2, cZ + cSize/2 }
            };

            if (CheckCollisionBoxes(playerBox, cubeBox)) {
                return Value(true);
            }
        }

        return Value(false);
    }

    Value native_pathfind(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("pathfind() requires start_pos, target_pos, map_data, and grid_size");

        std::vector<Value> startList = args[0].asList();
        std::vector<Value> targetList = args[1].asList();
        std::vector<Value> mapData = args[2].asList();
        float gridSize = (float)args[3].asFloat();

        Vector3 start = { (float)startList[0].asFloat(), (float)startList[1].asFloat(), (float)startList[2].asFloat() };
        Vector3 target = { (float)targetList[0].asFloat(), (float)targetList[1].asFloat(), (float)targetList[2].asFloat() };

        Vector3 bestDir = { target.x - start.x, 0, target.z - start.z };
        float len = sqrtf(bestDir.x * bestDir.x + bestDir.z * bestDir.z);
        
        if (len > 0.1f) {
            bestDir.x /= len;
            bestDir.z /= len;
        }

        Vector3 testPos = { start.x + bestDir.x * gridSize, start.y, start.z + bestDir.z * gridSize };
        
        BoundingBox nextBotBox = {
            (Vector3){ testPos.x - 1.0f, testPos.y - 0.5f, testPos.z - 1.0f },
            (Vector3){ testPos.x + 1.0f, testPos.y + 1.5f, testPos.z + 1.0f }
        };

        bool collision = false;
        for (const auto& objVal : mapData) {
            std::vector<Value> obj = objVal.asList();
            float s = (float)obj[3].asFloat();
            BoundingBox wallBox = {
                (Vector3){ (float)obj[0].asFloat() - s/2, (float)obj[1].asFloat() - s/2, (float)obj[2].asFloat() - s/2 },
                (Vector3){ (float)obj[0].asFloat() + s/2, (float)obj[1].asFloat() + s/2, (float)obj[2].asFloat() + s/2 }
            };

            if (CheckCollisionBoxes(nextBotBox, wallBox)) {
                collision = true;
                break;
            }
        }

        if (collision) {
            float angle = 0.785f; // 45 dərəcə
            Vector3 altDir = {
                bestDir.x * cosf(angle) - bestDir.z * sinf(angle),
                0,
                bestDir.x * sinf(angle) + bestDir.z * cosf(angle)
            };
            bestDir = altDir;
        }

        return Value(std::vector<Value>{ Value(bestDir.x), Value(bestDir.y), Value(bestDir.z) });
    }

    Value native_set_model_mesh_texture(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("set_model_texture_all() requires model_ptr and tex_ptr");

        Model* model = reinterpret_cast<Model*>(args[0].asInt());
        Texture2D* tex = reinterpret_cast<Texture2D*>(args[1].asInt());

        if (model && tex) {
            for (int i = 0; i < model->materialCount; i++) {
                model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
            }
            return Value(true);
        }
        return Value(false);
    }

    Value native_set_model_alpha_cutoff(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("set_alpha_cutoff() requires model_ptr and threshold (0.0 - 1.0)");

        Model* model = reinterpret_cast<Model*>(args[0].asInt());
        float cutoff = (float)args[1].asFloat();

        if (model) {
            for (int i = 0; i < model->materialCount; i++) {
                int loc = GetShaderLocation(model->materials[i].shader, "alphaCutoff");
                if (loc != -1) {
                    SetShaderValue(model->materials[i].shader, loc, &cutoff, SHADER_UNIFORM_FLOAT);
                }
            }
        }
        return Value();
    }

    Value native_set_alpha_discard(std::vector<Value>& args) {
        if (args.empty()) throw std::runtime_error("set_alpha_discard() requires model_ptr");

        Model* model = reinterpret_cast<Model*>(args[0].asInt());
        
        if (model) {
            for (int i = 0; i < model->materialCount; i++) {
                SetTextureFilter(model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture, TEXTURE_FILTER_POINT);
            }
            return Value(true);
        }
        return Value(false);
    }

    Value native_set_camera_roll(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("set_camera_roll() requires camera_ptr and angle");

        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        float rollAngle = (float)args[1].asFloat() * DEG2RAD;

        if (camera) {
            Vector3 forward = {
                camera->target.x - camera->position.x,
                camera->target.y - camera->position.y,
                camera->target.z - camera->position.z
            };
            forward = Vector3Normalize(forward);

            Vector3 worldUp = { 0.0f, 1.0f, 0.0f };

            Vector3 right = Vector3CrossProduct(forward, worldUp);
            right = Vector3Normalize(right);

            camera->up.x = worldUp.x * cosf(rollAngle) + right.x * sinf(rollAngle);
            camera->up.y = worldUp.y * cosf(rollAngle) + right.y * sinf(rollAngle);
            camera->up.z = worldUp.z * cosf(rollAngle) + right.z * sinf(rollAngle);
        }
        return Value();
    }

    Value native_rotate_yaw(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("rotate_yaw() requires camera_ptr and angle");

        Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
        float angle = (float)args[1].asFloat(); // dərəcə ilə

        if (camera) {
            UpdateCameraPro(camera, 
                (Vector3){ 0, 0, 0 },
                (Vector3){ angle, 0, 0 },
                0.0f
            );
        }
        return Value();
    }

    Value native_draw_instances(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("draw_instances() requires model_ptr and instances_list");

        Model* model = reinterpret_cast<Model*>(args[0].asInt());
        std::vector<Value> instances = args[1].asList();

        if (!model) return Value();

        for (const auto& instance_val : instances) {
            std::vector<Value> data = instance_val.asList();
            
            if (data.size() < 4) continue;

            Vector3 pos = { (float)data[0].asFloat(), (float)data[1].asFloat(), (float)data[2].asFloat() };
            float scale = (float)data[3].asFloat();
            
            // Opsional: rəng də əlavə edə bilərsən data[4]-ə
            Color col = WHITE;
            
            DrawModel(*model, pos, scale, col);
        }
        return Value();
    }

    Value native_draw_instances_ex(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("draw_instances() requires model_ptr, instances_list, cam_pos, and max_dist");

        Model* model = reinterpret_cast<Model*>(args[0].asInt());
        std::vector<Value> instances = args[1].asList();
        std::vector<Value> camPosList = args[2].asList();
        float maxDistSq = (float)args[3].asFloat() * (float)args[3].asFloat(); // Kvadratı ilə müqayisə daha sürətlidir (sqrt-dan qaçırıq)

        Vector3 camPos = { (float)camPosList[0].asFloat(), (float)camPosList[1].asFloat(), (float)camPosList[2].asFloat() };

        if (!model) return Value();

        for (const auto& instance_val : instances) {
            std::vector<Value> data = instance_val.asList();
            if (data.size() < 4) continue;

            float dx = (float)data[0].asFloat() - camPos.x;
            float dz = (float)data[2].asFloat() - camPos.z;
            float distSq = dx*dx + dz*dz; // Manhattan-a yaxın, amma daha dəqiq dairəvi məsafə

            if (distSq < maxDistSq) {
                Vector3 pos = { (float)data[0].asFloat(), (float)data[1].asFloat(), (float)data[2].asFloat() };
                float scale = (float)data[3].asFloat();
                DrawModel(*model, pos, scale, WHITE);
            }
        }
        return Value();
    }

    Value native_upload_persistent_group(std::vector<Value>& args) {
        if (args.size() < 2) throw std::runtime_error("upload_persistent_group() requires group_name and data_list");
        
        std::string groupName = args[0].asString();
        std::vector<Value> instances = args[1].asList();
        
        auto& group = persistent_groups[groupName];
        group.clear();

        for (const auto& inst_val : instances) {
            std::vector<Value> d = inst_val.asList();
            group.push_back({
                {(float)d[0].asFloat(), (float)d[1].asFloat(), (float)d[2].asFloat()},
                (float)d[3].asFloat()
            });
        }
        return Value(true);
    }

    Value native_draw_persistent_group(std::vector<Value>& args) {
        if (args.size() < 4) throw std::runtime_error("draw_persistent_group() requires name, model, cam_pos, max_dist");

        std::string groupName = args[0].asString();
        Model* model = reinterpret_cast<Model*>(args[1].asInt());
        std::vector<Value> camPosList = args[2].asList();
        float maxDistSq = (float)args[3].asFloat() * (float)args[3].asFloat();

        Vector3 camPos = {(float)camPosList[0].asFloat(), 0, (float)camPosList[2].asFloat()};

        if (persistent_groups.find(groupName) == persistent_groups.end()) return Value();

        const auto& instances = persistent_groups[groupName];
        for (const auto& inst : instances) {
            float dx = inst.position.x - camPos.x;
            float dz = inst.position.z - camPos.z;
            
            if ((dx*dx + dz*dz) < maxDistSq) {
                DrawModel(*model, inst.position, inst.scale, WHITE);
            }
        }
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
    vglib[pool.intern("line")]       = Value(VGLibNative::native_draw_line);
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
    vglib[pool.intern("load_font")] = Value(VGLibNative::native_load_font);
    vglib[pool.intern("text_ex")] = Value(VGLibNative::native_draw_text_ex);
    vglib[pool.intern("measure_text")] = Value(VGLibNative::native_measure_text_ex);
    vglib[pool.intern("grid")]        = Value(VGLibNative::native_draw_grid);
    vglib[pool.intern("update_camera")] = Value(VGLibNative::native_update_camera);
    vglib[pool.intern("disable_cursor")] = Value(VGLibNative::native_disable_cursor);
    vglib[pool.intern("enable_cursor")]  = Value(VGLibNative::native_enable_cursor);
    vglib[pool.intern("set_camera_height")] = Value(VGLibNative::native_set_camera_height);
    vglib[pool.intern("move_forward")] = Value(VGLibNative::native_move_forward);
    vglib[pool.intern("move_right")]   = Value(VGLibNative::native_move_right);
    vglib[pool.intern("rotate_view")]   = Value(VGLibNative::native_rotate_view);
    vglib[pool.intern("get_fps")] = Value(VGLibNative::native_get_fps);
    vglib[pool.intern("load_shader")] = Value(VGLibNative::native_load_shader);
    vglib[pool.intern("begin_shader")] = Value(VGLibNative::native_begin_shader);
    vglib[pool.intern("end_shader")] = Value(VGLibNative::native_end_shader);
    vglib[pool.intern("set_shader_camera")] = Value(VGLibNative::native_set_shader_camera);
    vglib[pool.intern("check_collision")] = Value(VGLibNative::native_check_collision);
    vglib[pool.intern("get_pos")] = Value(VGLibNative::native_get_camera_pos);
    vglib[pool.intern("set_pos")] = Value(VGLibNative::native_set_camera_pos);
    vglib[pool.intern("get_ray_grid")] = Value(VGLibNative::native_get_ray_grid);
    vglib[pool.intern("load_render_texture")] = Value(VGLibNative::native_load_render_texture);
    vglib[pool.intern("begin_texture_mode")]  = Value(VGLibNative::native_begin_texture_mode);
    vglib[pool.intern("end_texture_mode")]    = Value(VGLibNative::native_end_texture_mode);
    vglib[pool.intern("draw_render_texture")] = Value(VGLibNative::native_draw_render_texture);
    vglib[pool.intern("load_model")] = Value(VGLibNative::native_load_model);
    vglib[pool.intern("set_model_texture")] = Value(VGLibNative::native_set_model_texture);
    vglib[pool.intern("draw_model")] = Value(VGLibNative::native_draw_model);
    vglib[pool.intern("set_shader_value")]    = Value(VGLibNative::native_set_shader_value);
    vglib[pool.intern("load_texture")]  = Value(VGLibNative::native_load_texture);
    vglib[pool.intern("cube_texture")]  = Value(VGLibNative::native_draw_cube_texture);
    vglib[pool.intern("plane_texture")] = Value(VGLibNative::native_draw_plane_texture);
    vglib[pool.intern("mouse_pos")] = Value(VGLibNative::native_get_mouse_pos);
    vglib[pool.intern("mouse_delta")] = Value(VGLibNative::native_get_mouse_delta);
    vglib[pool.intern("billboard")] = Value(VGLibNative::native_draw_billboard);
    vglib[pool.intern("load_map")]  = Value(VGLibNative::native_load_map);
    vglib[pool.intern("distance_3d")] = Value(VGLibNative::native_distance_3d);
    vglib[pool.intern("export_obj")] = Value(VGLibNative::native_export_obj);
    vglib[pool.intern("draw_texture")] = Value(VGLibNative::native_draw_texture);
    vglib[pool.intern("check_collision_map")] = Value(VGLibNative::native_check_collision_map);
    vglib[pool.intern("pathfind")] = Value(VGLibNative::native_pathfind);
    vglib[pool.intern("set_model_texture_all")] = Value(VGLibNative::native_set_model_mesh_texture);
    vglib[pool.intern("set_alpha_cutoff")]      = Value(VGLibNative::native_set_model_alpha_cutoff);
    vglib[pool.intern("set_alpha_discard")] = Value(VGLibNative::native_set_alpha_discard);
    vglib[pool.intern("set_roll")] = Value(VGLibNative::native_set_camera_roll);
    vglib[pool.intern("rotate_yaw")] = Value(VGLibNative::native_rotate_yaw);
    vglib[pool.intern("draw_instances")] = Value(VGLibNative::native_draw_instances);
    vglib[pool.intern("draw_instances_ex")] = Value(VGLibNative::native_draw_instances_ex);
    vglib[pool.intern("upload_persistent_group")] = Value(VGLibNative::native_upload_persistent_group);
    vglib[pool.intern("draw_persistent_group")] = Value(VGLibNative::native_draw_persistent_group);

    // VGLib properties
    vglib[pool.intern("version")]  = Value("v0.0.4-alpha").setReadOnly();

    // keyboard codes important
    // Numbers
    vglib[pool.intern("ONE")]    = Value(49).setReadOnly();
    vglib[pool.intern("TWO")]    = Value(50).setReadOnly();
    vglib[pool.intern("THREE")]  = Value(51).setReadOnly();
    vglib[pool.intern("FOUR")]   = Value(52).setReadOnly();
    vglib[pool.intern("FIVE")]   = Value(53).setReadOnly();
    vglib[pool.intern("SIX")]    = Value(54).setReadOnly();
    vglib[pool.intern("SEVEN")]  = Value(55).setReadOnly();
    vglib[pool.intern("EIGHT")]  = Value(56).setReadOnly();

    // editor movement for map builder
    vglib[pool.intern("I")] = Value(73).setReadOnly();
    vglib[pool.intern("K")] = Value(75).setReadOnly();
    vglib[pool.intern("J")] = Value(74).setReadOnly();
    vglib[pool.intern("L")] = Value(76).setReadOnly();
    vglib[pool.intern("U")] = Value(85).setReadOnly();
    vglib[pool.intern("O")] = Value(79).setReadOnly();
    vglib[pool.intern("Q")] = Value(81).setReadOnly();
    vglib[pool.intern("E")] = Value(69).setReadOnly();

    // Misc
    vglib[pool.intern("BACKSPACE")]  = Value(259).setReadOnly();
    vglib[pool.intern("SPACE")]      = Value(32).setReadOnly();
    vglib[pool.intern("ENTER")]      = Value(257).setReadOnly();
    vglib[pool.intern("ESCAPE")]     = Value(256).setReadOnly();
    vglib[pool.intern("LEFT_SHIFT")] = Value(340).setReadOnly();
    vglib[pool.intern("LEFT_CTRL")]  = Value(341).setReadOnly();
    
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
    vglib[pool.intern("GRAY")] = Value((int64_t)0x808080FF).setReadOnly();

    // video props
    vglib[pool.intern("FULLSCREEN")]      = Value(2).setReadOnly(); // FLAG_FULLSCREEN_WINDOW = 0x00000002
    vglib[pool.intern("VSYNC")]           = Value(64).setReadOnly(); // FLAG_VSYNC_HINT = 0x00000040
    vglib[pool.intern("CAMERA_FREE")]     = Value(0).setReadOnly();
    vglib[pool.intern("CAMERA_ORBITAL")]  = Value(2).setReadOnly();
    vglib[pool.intern("CAMERA_FIRST_P")]  = Value(3).setReadOnly();
    vglib[pool.intern("CAMERA_THIRD_P")]  = Value(4).setReadOnly();
}