#include "vglib_common.h"

namespace VGLibNative {

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

Value native_disable_cursor(std::vector<Value>& args) {
    DisableCursor();
    return Value();
}

Value native_enable_cursor(std::vector<Value>& args) {
    EnableCursor();
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

Value get_mouse_wheel(std::vector<Value>& args) {
    return Value((double)GetMouseWheelMove());
}

} // namespace VGLibNative