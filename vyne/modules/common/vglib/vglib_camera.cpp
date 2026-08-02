#include "vglib_common.h"
#include "vglib.h"

namespace VGLibNative {

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
    
    BeginMode3D(*camera);
    return Value();
}

Value native_end_3d_mode(std::vector<Value>& args) {
    EndMode3D();
    return Value();
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
        
        float len = sqrtf(forward.x * forward.x + forward.z * forward.z);
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
        
        float len = sqrtf(right.x * right.x + right.z * right.z);
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

Value native_get_camera_yaw(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("get_yaw() requires camera_ptr");

    Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
    
    if (camera) {
        float dx = camera->target.x - camera->position.x;
        float dz = camera->target.z - camera->position.z;
        
        float yaw = atan2f(dz, dx) * RAD2DEG;
        
        return Value((double)yaw);
    }
    return Value(0.0);
}

Value native_rotate_yaw(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("rotate_yaw() requires camera_ptr and angle");

    Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
    float angle = (float)args[1].asFloat();

    if (camera) {
        UpdateCameraPro(camera, 
            (Vector3){ 0, 0, 0 },
            (Vector3){ angle, 0, 0 },
            0.0f
        );
    }
    return Value();
}

} // namespace VGLibNative