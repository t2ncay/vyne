ruleset { dynamic_casting };
module vglib;
module vaudio;

vglib.init(1920, 1080, 100, "Vyne Pro - VHS Horror Stack", vglib.FULLSCREEN);
camera = vglib.camera();
vglib.set_pos(camera, 0.0, 1.8, 0.0);

vglib.disable_cursor();

# shaders - CHROMA SİLİNDİ
fog_shader = vglib.load_shader("tests/graphics/shaders/fog.vs", "tests/graphics/shaders/fog.fs");
vhs_shader = vglib.load_shader("tests/graphics/shaders/vhs_horror.fs");

# textures
building_tex = vglib.load_texture("tests/assets/building.jpg");
ground_tex = vglib.load_texture("tests/assets/asphalt.jpg");

# Artıq tək render target kifayətdir (screen -> vhs -> screen)
screen_target = vglib.load_render_texture(1920, 1080);

player_size = [0.8, 1.8, 0.8];
walls = [
    [-7.0,  5.0, 10.0, 10.0],
    [-7.0,  8.0, 22.0, 16.0],
    [-10.0, 5.0, 35.0, 10.0],
    [-12.0, 12.0, 50.0, 24.0],

    [7.0,   5.0, 12.0, 10.0],
    [9.0,   7.0, 25.0, 14.0],
    [7.0,   5.0, 38.0, 10.0],
    [15.0,  15.0, 55.0, 30.0],

    [0.0,   5.0, -10.0, 15.0],

    [25.0,  5.0, 40.0, 10.0],
    [35.0,  8.0, 40.0, 16.0],
    [45.0,  5.0, 30.0, 10.0],

    [0.0,   25.0, 90.0, 50.0],
    
    [-25.0, 2.5, 20.0, 5.0],
    [20.0,  2.5, 10.0, 5.0],
    [-30.0, 10.0, 80.0, 20.0]
];

run_time = 0.0;

mouse_sens = 0.15;
speed = 0.15;
normal_height = 1.8;
crouch_height = 0.9;

current_y = normal_height;
velocity_y = 0.0;
gravity = -0.012;
jump_force = 0.35;
is_grounded = true;

vaudio.init_audio();
volume :: Float64 = 1.0;
vaudio.volume(volume);
ambiance = vaudio.load_sound("tests/assets/akira.wav");

vaudio.sound_volume(ambiance, 1.0);
vaudio.play_sound(ambiance);

while (vglib.running()) {
    run_time = run_time + 0.016;
    vglib.rotate_view(camera, 0.15);

    # --- PHYSICS & COLLISION LOGIC (TOXUNULMADI) ---
    if (vglib.key_down(vglib.LEFT_SHIFT)) {
        current_speed = speed / 4.0;
        target_h = crouch_height;
    } else {
        current_speed = speed;
        target_h = normal_height;
    }

    if (vglib.key_down(vglib.SPACE)) {
        if (is_grounded) {
            velocity_y = jump_force;
            is_grounded = false;
        }
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
        current_y = target_h;
    }

    vglib.set_camera_height(camera, current_y);

    if (vglib.key_down(vglib.W)) { 
        vglib.move_forward(camera, current_speed); 
        through wall :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), player_size, wall, wall[3])) {
                vglib.move_forward(camera, -current_speed);
            }
        };
    }
    if (vglib.key_down(vglib.S)) { 
        vglib.move_forward(camera, current_speed * -1.0); 
        through wall :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), player_size, wall, wall[3])) {
                vglib.move_forward(camera, current_speed);
            }
        };
    }
    if (vglib.key_down(vglib.A)) { 
        vglib.move_right(camera, current_speed * -1.0); 
        through wall :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), player_size, wall, wall[3])) {
                vglib.move_right(camera, current_speed);
            }
        };
    }
    if (vglib.key_down(vglib.D)) { 
        vglib.move_right(camera, current_speed); 
        through wall :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), player_size, wall, wall[3])) {
                vglib.move_right(camera, -current_speed);
            }
        };
    }

    vglib.begin_texture_mode(screen_target);
        vglib.clear(vglib.rgba(128, 128, 140, 255));
        
        vglib.begin3d(camera);
            vglib.set_shader_camera(fog_shader, camera);
            vglib.begin_shader(fog_shader);
                vglib.plane_texture(ground_tex, 0.0, 0.0, 0.0, 200.0, 200.0);
                through w :: walls -> loop {
                    vglib.cube_texture(building_tex, w[0], w[1], w[2], w[3], vglib.WHITE);
                };
            vglib.end_shader();
        vglib.end3d();
    vglib.end_texture_mode();

    vglib.begin();
        vglib.clear(vglib.BLACK);
        vglib.set_shader_value(vhs_shader, "time", run_time);
        vglib.begin_shader(vhs_shader);
            vglib.draw_render_texture(screen_target);
        vglib.end_shader();

        # UI
        vglib.text("POST-PROCESS: VHS ACTIVE", 40, 40, 25, vglib.RED);
        vglib.text("REC", 1800, 50, 30, vglib.RED);
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vaudio.close_audio();
vglib.close();