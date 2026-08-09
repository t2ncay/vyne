#include "vglib_common.h"

std::map<std::string, std::vector<PersistentInstance>> persistent_groups;

namespace VGLibNative {
    // Declarations for linking symbol bindings
    Value native_donut(std::vector<Value>& args);
    Value native_init(std::vector<Value>& args);
    Value native_is_running(std::vector<Value>& args);
    Value native_begin_frame(std::vector<Value>& args);
    Value native_end_frame(std::vector<Value>& args);
    Value native_clear(std::vector<Value>& args);
    Value native_draw_line(std::vector<Value>& args);
    Value native_draw_line_3d(std::vector<Value>& args);
    Value native_draw_rect(std::vector<Value>& args);
    Value native_draw_circle(std::vector<Value>& args);
    Value native_close(std::vector<Value>& args);
    Value native_draw_cube(std::vector<Value>& args);
    Value native_init_3d_camera(std::vector<Value>& args);
    Value native_begin_3d_mode(std::vector<Value>& args);
    Value native_end_3d_mode(std::vector<Value>& args);
    Value native_is_key_down(std::vector<Value>& args);
    Value native_is_key_pressed(std::vector<Value>& args);
    Value native_get_char_pressed(std::vector<Value>& args);
    Value native_get_key_pressed(std::vector<Value>& args);
    Value native_is_mouse_button_down(std::vector<Value>& args);
    Value native_rgba(std::vector<Value>& args);
    Value native_draw_plane(std::vector<Value>& args);
    Value native_clear_gradient(std::vector<Value>& args);
    Value native_draw_text(std::vector<Value>& args);
    Value native_load_font(std::vector<Value>& args);
    Value native_draw_text_ex(std::vector<Value>& args);
    Value native_measure_text_ex(std::vector<Value>& args);
    Value native_draw_grid(std::vector<Value>& args);
    Value native_update_camera(std::vector<Value>& args);
    Value native_disable_cursor(std::vector<Value>& args);
    Value native_enable_cursor(std::vector<Value>& args);
    Value native_set_camera_height(std::vector<Value>& args);
    Value native_move_forward(std::vector<Value>& args);
    Value native_move_right(std::vector<Value>& args);
    Value native_rotate_view(std::vector<Value>& args);
    Value native_get_fps(std::vector<Value>& args);
    Value native_load_shader(std::vector<Value>& args);
    Value native_begin_shader(std::vector<Value>& args);
    Value native_end_shader(std::vector<Value>& args);
    Value native_set_shader_camera(std::vector<Value>& args);
    Value native_check_collision(std::vector<Value>& args);
    Value native_get_camera_pos(std::vector<Value>& args);
    Value native_set_camera_pos(std::vector<Value>& args);
    Value native_get_ray_grid(std::vector<Value>& args);
    Value native_load_render_texture(std::vector<Value>& args);
    Value native_begin_texture_mode(std::vector<Value>& args);
    Value native_end_texture_mode(std::vector<Value>& args);
    Value native_draw_render_texture(std::vector<Value>& args);
    Value native_load_model(std::vector<Value>& args);
    Value native_set_model_texture(std::vector<Value>& args);
    Value native_draw_model(std::vector<Value>& args);
    Value native_set_shader_value(std::vector<Value>& args);
    Value native_load_texture(std::vector<Value>& args);
    Value native_draw_cube_texture(std::vector<Value>& args);
    Value native_draw_plane_texture(std::vector<Value>& args);
    Value native_get_mouse_pos(std::vector<Value>& args);
    Value native_get_mouse_delta(std::vector<Value>& args);
    Value native_draw_billboard(std::vector<Value>& args);
    Value native_load_map(std::vector<Value>& args);
    Value native_distance_3d(std::vector<Value>& args);
    Value native_export_obj(std::vector<Value>& args);
    Value native_draw_texture(std::vector<Value>& args);
    Value native_check_collision_map(std::vector<Value>& args);
    Value native_pathfind(std::vector<Value>& args);
    Value native_set_model_mesh_texture(std::vector<Value>& args);
    Value native_set_model_alpha_cutoff(std::vector<Value>& args);
    Value native_set_alpha_discard(std::vector<Value>& args);
    Value native_set_camera_roll(std::vector<Value>& args);
    Value native_rotate_yaw(std::vector<Value>& args);
    Value native_get_camera_yaw(std::vector<Value>& args);
    Value native_draw_instances(std::vector<Value>& args);
    Value native_draw_instances_ex(std::vector<Value>& args);
    Value native_upload_persistent_group(std::vector<Value>& args);
    Value native_draw_persistent_group(std::vector<Value>& args);
    Value get_mouse_wheel(std::vector<Value>& args);
    Value native_draw_phase_scope(std::vector<Value>& args);
    Value native_draw_canvas_scaled(std::vector<Value>& args);
    Value native_draw_spectrum_analyzer(std::vector<Value>& args);
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
    vglib[pool.intern("line_3d")]    = Value(VGLibNative::native_draw_line_3d);
    vglib[pool.intern("rect")]       = Value(VGLibNative::native_draw_rect);
    vglib[pool.intern("circle")]     = Value(VGLibNative::native_draw_circle);
    vglib[pool.intern("close")]      = Value(VGLibNative::native_close);
    vglib[pool.intern("cube")]       = Value(VGLibNative::native_draw_cube);
    vglib[pool.intern("camera")]     = Value(VGLibNative::native_init_3d_camera);
    vglib[pool.intern("begin3d")]    = Value(VGLibNative::native_begin_3d_mode);
    vglib[pool.intern("end3d")]      = Value(VGLibNative::native_end_3d_mode);
    vglib[pool.intern("key_down")]     = Value(VGLibNative::native_is_key_down);
    vglib[pool.intern("key_pressed")]  = Value(VGLibNative::native_is_key_pressed);
    vglib[pool.intern("get_char")]     = Value(VGLibNative::native_get_char_pressed);
    vglib[pool.intern("get_key")]      = Value(VGLibNative::native_get_key_pressed);
    vglib[pool.intern("mouse_down")]   = Value(VGLibNative::native_is_mouse_button_down);
    vglib[pool.intern("mouse_pos")]    = Value(VGLibNative::native_get_mouse_pos);
    vglib[pool.intern("mouse_delta")]  = Value(VGLibNative::native_get_mouse_delta);
    vglib[pool.intern("mouse_wheel")]  = Value(VGLibNative::get_mouse_wheel);
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
    vglib[pool.intern("get_yaw")] = Value(VGLibNative::native_get_camera_yaw);
    vglib[pool.intern("draw_instances")] = Value(VGLibNative::native_draw_instances);
    vglib[pool.intern("draw_instances_ex")] = Value(VGLibNative::native_draw_instances_ex);
    vglib[pool.intern("upload_persistent_group")] = Value(VGLibNative::native_upload_persistent_group);
    vglib[pool.intern("draw_persistent_group")] = Value(VGLibNative::native_draw_persistent_group);
    vglib[pool.intern("mouse_wheel")] = Value(VGLibNative::get_mouse_wheel);
    vglib[pool.intern("draw_phase_scope")] = Value(VGLibNative::native_draw_phase_scope);
    vglib[pool.intern("draw_canvas_scaled")] = Value(VGLibNative::native_draw_canvas_scaled);
    vglib[pool.intern("draw_spectrum_analyzer")] = Value(VGLibNative::native_draw_spectrum_analyzer);

    // VGLib properties
    vglib[pool.intern("version")]  = Value("v0.0.4-alpha").setReadOnly();

    // Numbers (0-9)
    vglib[pool.intern("ZERO")]   = Value(48).setReadOnly();
    vglib[pool.intern("ONE")]    = Value(49).setReadOnly();
    vglib[pool.intern("TWO")]    = Value(50).setReadOnly();
    vglib[pool.intern("THREE")]  = Value(51).setReadOnly();
    vglib[pool.intern("FOUR")]   = Value(52).setReadOnly();
    vglib[pool.intern("FIVE")]   = Value(53).setReadOnly();
    vglib[pool.intern("SIX")]    = Value(54).setReadOnly();
    vglib[pool.intern("SEVEN")]  = Value(55).setReadOnly();
    vglib[pool.intern("EIGHT")]  = Value(56).setReadOnly();
    vglib[pool.intern("NINE")]   = Value(57).setReadOnly();

    // Alphabet (A-Z)
    vglib[pool.intern("A")] = Value(65).setReadOnly();
    vglib[pool.intern("B")] = Value(66).setReadOnly();
    vglib[pool.intern("C")] = Value(67).setReadOnly();
    vglib[pool.intern("D")] = Value(68).setReadOnly();
    vglib[pool.intern("E")] = Value(69).setReadOnly();
    vglib[pool.intern("F")] = Value(70).setReadOnly();
    vglib[pool.intern("G")] = Value(71).setReadOnly();
    vglib[pool.intern("H")] = Value(72).setReadOnly();
    vglib[pool.intern("I")] = Value(73).setReadOnly();
    vglib[pool.intern("J")] = Value(74).setReadOnly();
    vglib[pool.intern("K")] = Value(75).setReadOnly();
    vglib[pool.intern("L")] = Value(76).setReadOnly();
    vglib[pool.intern("M")] = Value(77).setReadOnly();
    vglib[pool.intern("N")] = Value(78).setReadOnly();
    vglib[pool.intern("O")] = Value(79).setReadOnly();
    vglib[pool.intern("P")] = Value(80).setReadOnly();
    vglib[pool.intern("Q")] = Value(81).setReadOnly();
    vglib[pool.intern("R")] = Value(82).setReadOnly();
    vglib[pool.intern("S")] = Value(83).setReadOnly();
    vglib[pool.intern("T")] = Value(84).setReadOnly();
    vglib[pool.intern("U")] = Value(85).setReadOnly();
    vglib[pool.intern("V")] = Value(86).setReadOnly();
    vglib[pool.intern("W")] = Value(87).setReadOnly();
    vglib[pool.intern("X")] = Value(88).setReadOnly();
    vglib[pool.intern("Y")] = Value(89).setReadOnly();
    vglib[pool.intern("Z")] = Value(90).setReadOnly();

    // Control & Navigation
    vglib[pool.intern("BACKSPACE")]   = Value(259).setReadOnly();
    vglib[pool.intern("TAB")]         = Value(258).setReadOnly();
    vglib[pool.intern("ENTER")]       = Value(257).setReadOnly();
    vglib[pool.intern("ESCAPE")]      = Value(256).setReadOnly();
    vglib[pool.intern("SPACE")]       = Value(32).setReadOnly();
    vglib[pool.intern("DELETE")]      = Value(261).setReadOnly();
    vglib[pool.intern("CAPS_LOCK")]   = Value(280).setReadOnly();

    // Modifier Keys
    vglib[pool.intern("LEFT_SHIFT")]  = Value(340).setReadOnly();
    vglib[pool.intern("LEFT_CTRL")]   = Value(341).setReadOnly();
    vglib[pool.intern("LEFT_ALT")]    = Value(342).setReadOnly();
    vglib[pool.intern("RIGHT_SHIFT")] = Value(344).setReadOnly();
    vglib[pool.intern("RIGHT_CTRL")]  = Value(345).setReadOnly();
    vglib[pool.intern("RIGHT_ALT")]   = Value(346).setReadOnly();

    // Arrow Navigation
    vglib[pool.intern("UP")]    = Value(265).setReadOnly();
    vglib[pool.intern("DOWN")]  = Value(264).setReadOnly();
    vglib[pool.intern("LEFT")]  = Value(263).setReadOnly();
    vglib[pool.intern("RIGHT")] = Value(262).setReadOnly();

    // Function Keys
    vglib[pool.intern("F1")]  = Value(290).setReadOnly();
    vglib[pool.intern("F2")]  = Value(291).setReadOnly();
    vglib[pool.intern("F3")]  = Value(292).setReadOnly();
    vglib[pool.intern("F4")]  = Value(293).setReadOnly();
    vglib[pool.intern("F5")]  = Value(294).setReadOnly();
    vglib[pool.intern("F6")]  = Value(295).setReadOnly();
    vglib[pool.intern("F7")]  = Value(296).setReadOnly();
    vglib[pool.intern("F8")]  = Value(297).setReadOnly();
    vglib[pool.intern("F9")]  = Value(298).setReadOnly();
    vglib[pool.intern("F10")] = Value(299).setReadOnly();
    vglib[pool.intern("F11")] = Value(300).setReadOnly();
    vglib[pool.intern("F12")] = Value(301).setReadOnly();

    // Mouse
    vglib[pool.intern("MOUSE_LEFT")]   = Value(0).setReadOnly();
    vglib[pool.intern("MOUSE_RIGHT")]  = Value(1).setReadOnly();
    vglib[pool.intern("MOUSE_MIDDLE")] = Value(2).setReadOnly();

    // Preset colors
    vglib[pool.intern("WHITE")]   = Value((int64_t)0xFFFFFFFF);
    vglib[pool.intern("BLACK")]   = Value((int64_t)0x000000FF).setReadOnly();
    vglib[pool.intern("RED")]     = Value((int64_t)0xFF0000FF);
    vglib[pool.intern("GREEN")]   = Value((int64_t)0x00FF00FF).setReadOnly();
    vglib[pool.intern("BLUE")]    = Value((int64_t)0x0000FFFF).setReadOnly();
    vglib[pool.intern("CYAN")]    = Value((int64_t)0x00AAFFFF).setReadOnly();
    vglib[pool.intern("MAGENTA")] = Value((int64_t)0xFF00FFFF).setReadOnly();
    vglib[pool.intern("PURPLE")]  = Value((int64_t)0x800080FF).setReadOnly();
    vglib[pool.intern("GRAY")]    = Value((int64_t)0x808080FF).setReadOnly();

    // Video & Camera modes
    vglib[pool.intern("FULLSCREEN")]      = Value(2).setReadOnly();
    vglib[pool.intern("VSYNC")]           = Value(64).setReadOnly();
    vglib[pool.intern("CAMERA_FREE")]     = Value(0).setReadOnly();
    vglib[pool.intern("CAMERA_ORBITAL")]  = Value(2).setReadOnly();
    vglib[pool.intern("CAMERA_FIRST_P")]  = Value(3).setReadOnly();
    vglib[pool.intern("CAMERA_THIRD_P")]  = Value(4).setReadOnly();
}