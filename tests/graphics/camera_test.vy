ruleset { dynamic_casting };
module vglib;
module vaudio;

vglib.init(1920, 1080, 100, "Vyne Pro - Dual Shader System", vglib.FULLSCREEN);
camera = vglib.camera();
vglib.disable_cursor();

fog_shader = vglib.load_shader("tests/graphics/shaders/fog.fs");
vhs_shader = vglib.load_shader("tests/graphics/shaders/vhs_horror.fs");

screen_target = vglib.load_render_texture(1920, 1080);

player_size = [0.8, 1.8, 0.8];
walls = [[0.0, 2.5, 10.0, 5.0], [10.0, 2.5, 20.0, 5.0], [-15.0, 2.5, 35.0, 5.0]];

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
                vglib.grid(100, 1.0);
                through w :: walls -> loop {
                    vglib.cube(w[0], w[1], w[2], w[3], 0.0, vglib.CYAN);
                };
            vglib.end_shader();
        vglib.end3d();
    vglib.end_texture_mode();

    vglib.begin();
        vglib.set_shader_value(vhs_shader, "time", run_time);
        
        vglib.begin_shader(vhs_shader);
            vglib.draw_render_texture(screen_target);
        vglib.end_shader();

        vglib.text("SIGNAL STABILITY: CRITICAL", 40, 40, 25, vglib.rgba(255, 0, 0, 180));
        vglib.text("FPS: " + string(vglib.get_fps()), 40, 75, 20, vglib.BLACK);
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vglib.close();