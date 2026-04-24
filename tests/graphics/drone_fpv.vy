ruleset { dynamic_casting, warnings };

module vglib;
module vaudio;
module vmath;

vglib.init(1920, 1080, 75, "Drone FPV", vglib.FULLSCREEN + vglib.VSYNC);
camera = vglib.camera(90.0);

vglib.set_pos(camera, 30.0, 10.0, 0.0);
vglib.disable_cursor();

# --- SHADERS ---
vhs_color_shader = vglib.load_shader("shaders/vhs.fs");

# -- RESOURCES --
screen_target  = vglib.load_render_texture(1920, 1080);
vcr_font       = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

run_time = 0.0;

while (vglib.running()) {

    run_time = run_time + 0.016;
    
    vglib.begin();
        vglib.clear(vglib.BLACK);
        vglib.set_shader_value(vhs_color_shader, "time", run_time);
        vglib.set_shader_value(vhs_color_shader, "renderSize", [1920.0, 1080.0]);
        vglib.set_shader_value(vhs_color_shader, "offset", 0);
        vglib.begin_shader(vhs_color_shader);
            vglib.draw_render_texture(screen_target);
        vglib.end_shader();

        # Axon UI
        vglib.text_ex(vcr_font, "AXON BODY 3 - UNIT 402", 60, 60, 20, vglib.WHITE);
        if (vmath.sin(run_time * 4.0) > 0.0) { vglib.text_ex(vcr_font, "REC", 1800, 60, 25, vglib.RED); }
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vaudio.close_audio();
vglib.close();