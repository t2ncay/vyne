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

} // namespace VGLibNative