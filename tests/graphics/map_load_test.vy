ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1920, 1080, 75, "Vyne Pro - Bodycam Horror", vglib.FULLSCREEN + vglib.VSYNC);
camera = vglib.camera(80.0);
vglib.set_pos(camera, 30.0, 1.8, 0.0);

vglib.disable_cursor();

# --- SHADERS ---
fog_shader     = vglib.load_shader("tests/graphics/shaders/fog.vs", "tests/graphics/shaders/fog.fs");
vhs_shader     = vglib.load_shader("tests/graphics/shaders/vhs.fs");
vhs_color_shader = vglib.load_shader("tests/graphics/shaders/scanline.fs");
flashlight_shader = vglib.load_shader("tests/graphics/shaders/flashlight.fs");

# textures
tex_paths = [
    "tests/assets/Brick/Brick_16-512x512.png",
    "tests/assets/wall.jpeg", 
    "tests/assets/Dirt/Dirt_20-512x512.png", 
    "tests/assets/yusif.jpeg"
];

tex_slots = [];
through path :: tex_paths -> loop {
    tex_slots = tex_slots + [vglib.load_texture(path)];
};

ground_tex   = tex_slots[2];

out(tex_slots);

# --- RENDER TARGETS ---
screen_target  = vglib.load_render_texture(1920, 1080);
bodycam_target = vglib.load_render_texture(1920, 1080);

player_size = [0.8, 1.8, 0.8];
walls = vglib.load_map("tau_map.dat");

run_time = 0.0;
speed = 0.15;
sprint_multiplier = 2.5;
normal_height = 1.8;
crouch_height = 0.9;
current_y = 1.8;
velocity_y = 0.0;
gravity = -0.012;
jump_force = 0.35;

is_grounded = true;

vaudio.init_audio();
vaudio.volume(1.0);
ambiance = vaudio.load_sound("tests/assets/akira.wav");
vaudio.play_sound(ambiance);

while (vglib.running()) {
    run_time = run_time + 0.016;
    vglib.rotate_view(camera, 0.15);

    current_speed = speed;

    cam_pos = vglib.get_pos(camera);
    temp_ground_y = 0.0; # Default yer səviyyəsi

    through w :: walls -> loop {
        half = w[3] / 2.0;
        if (cam_pos[0] > w[0]-half && cam_pos[0] < w[0]+half && cam_pos[2] > w[2]-half && cam_pos[2] < w[2]+half) {
            top_y = w[1] + half;
            if (cam_pos[1] >= top_y - 0.8) { 
                if (top_y > temp_ground_y) { temp_ground_y = top_y; }
            }
        }
    };

    if (vglib.key_down(vglib.LEFT_CTRL)) {
        target_h = temp_ground_y + crouch_height;
        current_speed = 0.05;
    } else {
        target_h = temp_ground_y + normal_height;
    }

    if (vglib.key_down(vglib.LEFT_SHIFT)) {
        current_speed = current_speed * sprint_multiplier;
    }

    if (vglib.key_down(vglib.SPACE) && is_grounded) {
        velocity_y = jump_force;
        is_grounded = false;
    }

    if (is_grounded == false) {
        velocity_y = velocity_y + gravity;
        current_y = current_y + velocity_y;

        if (current_y <= target_h) {
            current_y = target_h;
            velocity_y = 0.0;
            is_grounded = true;
        }
    } else {
        diff = target_h - current_y;
        
        if (vmath.abs(diff) > 0.001) {
            if (diff < 0.0) {
                current_y = current_y + (diff * 0.1); 
            } else {
                current_y = current_y + (diff * 0.2); 
            }
        } else {
            current_y = target_h;
        }

        if (current_y > target_h + 0.5) {
            is_grounded = false;
        }
    }

    vglib.rotate_view(camera, 0.15);
    vglib.set_camera_height(camera, current_y);

    old_pos = vglib.get_pos(camera);
    
    moved = false;
    if (vglib.key_down(vglib.W)) { vglib.move_forward(camera, current_speed); moved = true; }
    if (vglib.key_down(vglib.S)) { vglib.move_forward(camera, current_speed * -1.0); moved = true; }
    if (vglib.key_down(vglib.A)) { vglib.move_right(camera, current_speed * -1.0); moved = true; }
    if (vglib.key_down(vglib.D)) { vglib.move_right(camera, current_speed); moved = true; }

    new_pos = vglib.get_pos(camera);

    if (moved || !is_grounded) {
        if (vglib.check_collision_map(new_pos, player_size, walls)) {
            
            if (velocity_y > 0.0) {
                test_pos_y = [old_pos[0], new_pos[1], old_pos[2]];
                if (vglib.check_collision_map(test_pos_y, player_size, walls)) {
                    velocity_y = 0.0; # Başın tavana dəydi, impulsu sıfırla
                    current_y = old_pos[1]; # Köhnə hündürlükdə qal
                    vglib.set_pos(camera, old_pos[0], old_pos[1], old_pos[2]);
                }
            }

            test_x = [new_pos[0], old_pos[1], old_pos[2]];
            if (!vglib.check_collision_map(test_x, player_size, walls)) {
                vglib.set_pos(camera, test_x[0], old_pos[1], test_x[2]);
            } else {
                test_pos_z = [old_pos[0], old_pos[1], new_pos[2]];
                if (!vglib.check_collision_map(test_pos_z, player_size, walls)) {
                    vglib.set_pos(camera, test_pos_z[0], old_pos[1], test_pos_z[2]);
                } else {
                    vglib.set_pos(camera, old_pos[0], old_pos[1], old_pos[2]);
                }
            }
        }
    }

    vglib.begin_texture_mode(screen_target);
        vglib.clear(vglib.rgba(128, 128, 140, 255));
        vglib.begin3d(camera);
            vglib.set_shader_camera(fog_shader, camera);
            vglib.begin_shader(fog_shader);
                vglib.plane_texture(ground_tex, 0.0, 0.0, 0.0, 200.0, 200.0);
                through w :: walls -> loop {
                    tex_idx = 0;
                    if (w.size() > 4) {
                        tex_idx = int64(w[4]);
                    }
                                        
                    vglib.cube_texture(tex_slots[tex_idx], w[0], w[1], w[2], w[3], vglib.WHITE);
                };
            vglib.end_shader();
        vglib.end3d();
    vglib.end_texture_mode();

    vglib.begin_texture_mode(bodycam_target);
        vglib.clear(vglib.BLACK);
        vglib.set_shader_value(vhs_shader, "time", run_time);
        vglib.set_shader_value(vhs_shader, "renderSize", [1920.0, 1080.0]);
        vglib.begin_shader(vhs_shader);
            vglib.draw_render_texture(screen_target);
        vglib.end_shader();
    vglib.end_texture_mode();

    vglib.begin();
        vglib.clear(vglib.BLACK);
        vglib.set_shader_value(vhs_color_shader, "time", run_time);
        vglib.set_shader_value(vhs_color_shader, "renderSize", [1920.0, 1080.0]);
        vglib.begin_shader(vhs_color_shader);
            vglib.draw_render_texture(bodycam_target);
        vglib.end_shader();

        # UI Overlay
        vglib.text("AXON BODY 3 - UNIT 402", 60, 60, 20, vglib.WHITE);
        vglib.text("2026-04-21 01:14:23", 60, 90, 18, vglib.WHITE);
        vglib.text("REC", 1800, 60, 25, vglib.RED);
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vaudio.close_audio();
vglib.close();