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

vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

# textures
building_tex = vglib.load_texture("tests/assets/building.jpg");
ground_tex   = vglib.load_texture("tests/assets/asphalt_road_3.jpg");

# --- RENDER TARGETS ---
screen_target  = vglib.load_render_texture(1920, 1080);
bodycam_target = vglib.load_render_texture(1920, 1080);
flashlight_target = vglib.load_render_texture(1920, 1080);

# --- MODEL LOADING ---
cat_model = vglib.load_model("tests/assets/12222_Cat_v1_l3.obj");
cat_tex   = vglib.load_texture("tests/assets/Cat_diffuse.jpg");

vglib.set_model_texture(cat_model, cat_tex);

cat_scale = 0.05;
cat_x = 35.0;
cat_z = 25.0;

airplane_model = vglib.load_model("tests/assets/11803_Airplane_v1_l1.obj");
airplane_tex   = vglib.load_texture("tests/assets/11803_Airplane_body_diff.jpg"); # Əgər teksturası varsa

vglib.set_model_texture(airplane_model, airplane_tex);

air_z = -50.0; 
air_x = -330.0;
air_y = 100.0;
air_scale = 0.02;
air_speed = 0.8;

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

    [0.0,  25.0, 90.0, 50.0],
    
    [-25.0, 2.5, 20.0, 5.0],
    [20.0,  2.5, 10.0, 5.0],
    [-30.0, 10.0, 80.0, 20.0]
];

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
    hover = vmath.sin(run_time * 2.0) * 0.2;

    cam_pos = vglib.get_pos(camera);
    temp_ground_y = 0.0;

    through w :: walls -> loop {
        half  = w[3] / 2.0;
        min_x = w[0] - half;
        max_x = w[0] + half;
        min_z = w[2] - half;
        max_z = w[2] + half;

        if (cam_pos[0] > min_x && cam_pos[0] < max_x && cam_pos[2] > min_z && cam_pos[2] < max_z) {
            top_y = w[1] + half;
            
            if (cam_pos[1] >= top_y - 0.5) {
                temp_ground_y = top_y;
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

                vglib.draw_model(
                    cat_model, 
                    cat_x, 
                    2.5 + hover, 
                    cat_z, 
                    cat_scale,
                    vglib.WHITE
                );

                cat_x = cat_x - run_time / 100;

                air_x = air_x + air_speed;

                vglib.draw_model(
                    airplane_model, 
                    air_x, 
                    air_y, 
                    air_z, 
                    air_scale, 
                    vglib.WHITE
                );
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
        vglib.text_ex(vcr_font,"AXON BODY 3 - UNIT 402", 60, 60, 20, vglib.WHITE);
        vglib.text_ex(vcr_font,"2026-04-21 01:14:23", 60, 90, 18, vglib.WHITE);
        vglib.text_ex(vcr_font,"REC", 1800, 60, 25, vglib.RED);
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vaudio.close_audio();
vglib.close();