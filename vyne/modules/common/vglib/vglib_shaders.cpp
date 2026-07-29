#include "vglib_common.h"

namespace VGLibNative {

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

} // namespace VGLibNative