ruleset { dynamic_casting, warnings };

module vglib;
module vaudio;
module vmath;

# --- SETUP ---
vglib.init(1920, 1080, 75, "Drone FPV - Kharkiv Operation", vglib.FULLSCREEN + vglib.VSYNC);
camera = vglib.camera(90.0);
vglib.set_pos(camera, 0.0, 40.0, -20.0);
vglib.disable_cursor();

# --- SHADERS & FONTS ---
vhs_color_shader = vglib.load_shader("shaders/vhs.fs");
vcr_font         = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

# --- ASSETS (Map-də istifadə olunan teksturalar) ---
tex_paths = [
    "tests/assets/Brick/Brick_16-512x512.png", # 0
    "tests/assets/wall.jpeg",                  # 1
    "tests/assets/building.jpg",              # 2
    "tests/assets/yusif.jpeg",                 # 3
    "tests/assets/asphalt_road_3.jpg",         # 4
    "tests/assets/Metal/Metal_18-512x512.png", # 5
    "tests/assets/Metal/Metal_18-512x512.png", # 6 (Ehtiyat üçün)
    "tests/assets/Metal/Metal_18-512x512.png"  # 7 (Xətanı aradan qaldıracaq)
];

tex_slots = [];
through path :: tex_paths -> loop {
    tex_slots = tex_slots + [vglib.load_texture(path)];
};

# --- MAP DATA ---
map_data = vglib.load_map("tau_map.dat");
screen_target = vglib.load_render_texture(1920, 1080);

# --- STATE ---
run_time = 0.0;
intro_time = 0.0;
intro_finished = false;
ui_glitch_factor = 1.0;

while (vglib.running()) {
    run_time = run_time + 0.016;

    # 1. Kadrı Gizli Render Target-ə çəkirik (Map burada render olunur)
    vglib.begin_texture_mode(screen_target);
        vglib.clear(vglib.rgba(20, 20, 25, 255));
        vglib.begin3d(camera);
            vglib.grid(100, 2.0); # Yeri göstərmək üçün
            
            # Sənin Architect xəritəni bura render edirik
            through obj :: map_data -> loop {
                # obj[4] tekstura slotudur, obj[0..2] isə koordinatlar
                vglib.cube_texture(tex_slots[obj[4]], obj[0], obj[1], obj[2], obj[3], vglib.WHITE);
            };
        vglib.end3d();
    vglib.end_texture_mode();

    # 2. Əsas Ekran Renderi
    vglib.begin();
        vglib.clear(vglib.BLACK);

        if (intro_finished == false) {
            intro_time = intro_time + 0.016;
            intro_alpha = 0;

            if (intro_time < 3.0) {
                intro_alpha = int64((intro_time / 3.0) * 255.0);
            } else if (intro_time < 12.0) {
                intro_alpha = 255;
            } else {
                intro_finished = true;
            }

            t_color = vglib.rgba(255, 255, 255, intro_alpha);
            vglib.text_ex(vcr_font, "KHARKIV, UKRAINE", 780, 500, 40, t_color);
            vglib.text_ex(vcr_font, "APRIL 25, 2026", 850, 560, 20, t_color);
        } 
        else {
            # --- DRON AKTİV ---
            if (ui_glitch_factor > 0.0) { ui_glitch_factor = ui_glitch_factor - 0.005; }

            # Map Renderini Shader ilə Ekrana veririk
            vglib.set_shader_value(vhs_color_shader, "time", run_time);
            vglib.set_shader_value(vhs_color_shader, "offset", ui_glitch_factor * 0.15);
            vglib.begin_shader(vhs_color_shader);
                vglib.draw_render_texture(screen_target);
            vglib.end_shader();

            # --- OSD (Dron İnterfeysi) ---
            ui_color = vglib.rgba(255, 255, 255, 180);
            ui_glitch = vmath.sin(run_time * 60.0) * (ui_glitch_factor * 8.0);

            vglib.text_ex(vcr_font, "MODE: FPV ACRO", 60 + ui_glitch, 60, 20, ui_color);
            vglib.text_ex(vcr_font, "BAT: 22.4V", 1700 + ui_glitch, 60, 20, vglib.GREEN);
            
            # Crosshair
            vglib.line(940, 540, 980, 540, ui_color);
            vglib.line(960, 520, 960, 560, ui_color);

            # Sadə Dron Hərəkəti (WASD)
            fly_speed = 0.3;
            vglib.rotate_view(camera, 0.1);
            if (vglib.key_down(vglib.W)) { vglib.move_forward(camera, fly_speed); }
            if (vglib.key_down(vglib.S)) { vglib.move_forward(camera, -fly_speed); }
            if (vglib.key_down(vglib.A)) { vglib.move_right(camera, -fly_speed); }
            if (vglib.key_down(vglib.D)) { vglib.move_right(camera, fly_speed); }
        }

        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vglib.close();