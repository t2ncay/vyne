ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1920, 1080, 75, "Vyne Pro - Selena Nextbot", vglib.FULLSCREEN + vglib.VSYNC);
camera = vglib.camera(80.0);
vglib.set_pos(camera, 30.0, 1.8, 0.0);
vglib.disable_cursor();

# --- SHADERS ---
fog_shader       = vglib.load_shader("tests/graphics/shaders/fog.vs", "tests/graphics/shaders/fog.fs");
vhs_shader       = vglib.load_shader("tests/graphics/shaders/vhs.fs");
vhs_color_shader = vglib.load_shader("tests/graphics/shaders/scanline.fs");
vcr_font         = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

# --- TEXTURES ---
building_tex = vglib.load_texture("tests/assets/building.jpg");
ground_tex   = vglib.load_texture("tests/assets/asphalt_road_3.jpg");
nextbot_tex  = vglib.load_texture("tests/assets/selena.jpg");

# --- SFX ---
vaudio.init_audio();
ambiance    = vaudio.load_sound("tests/assets/vhs_sound.wav");
qulaq       = vaudio.load_sound("tests/assets/selena.wav");
death_sound = vaudio.load_sound("tests/assets/death_sound.wav");
vaudio.volume(1.0);

# --- RENDER TARGETS ---
screen_target  = vglib.load_render_texture(1920, 1080);
bodycam_target = vglib.load_render_texture(1920, 1080);

# --- PHYSICS & WORLD ---
player_size = [0.8, 1.8, 0.8];
p_size = [player_size[0] + 0.1, player_size[1], player_size[2] + 0.1];
walls = [
    [-7.0, 5.0, 10.0, 10.0], [-7.0, 8.0, 22.0, 16.0], [-10.0, 5.0, 35.0, 10.0],
    [-12.0, 12.0, 50.0, 24.0], [7.0, 5.0, 12.0, 10.0], [9.0, 7.0, 25.0, 14.0],
    [7.0, 5.0, 38.0, 10.0], [15.0, 15.0, 55.0, 30.0], [0.0, 5.0, -10.0, 15.0],
    [25.0, 5.0, 40.0, 10.0], [35.0, 8.0, 40.0, 16.0], [45.0, 5.0, 30.0, 10.0],
    [0.0, 25.0, 90.0, 50.0], [-25.0, 2.5, 20.0, 5.0], [20.0, 2.5, 10.0, 5.0],
    [-30.0, 10.0, 80.0, 20.0]
];

run_time = 0.0;
speed = 0.2;
sprint_multiplier = 2.5;
normal_height = 1.8;
crouch_height = 0.9;
current_y = 1.8;
velocity_y = 0.0;
gravity = -0.012;
jump_force = 0.5;

# --- GAME STATE ---
nextbot_pos = [60.0, 1.8, 60.0];
nextbot_speed = 0.4;
spawn_point = [30.0, 1.8, 0.0];
is_grounded = true;
is_dead = false;
death_timer = 0.0;
flash_timer = 0.0;
glitch_factor = 0.0;

# --- AI CONFIG ---
ai_tick_rate = 0.1;
ai_timer = 0.0;
target_x = 60.0;
target_z = 60.0;

title_size = vglib.measure_text(vcr_font, "SUBJECT LOST", 80);
sub_size   = vglib.measure_text(vcr_font, "SYSTEM ERROR: SIGNAL CORRUPTED", 30);

vaudio.play_sound(ambiance);

while (vglib.running()) {
    run_time = run_time + 0.016;
    vglib.rotate_view(camera, 0.15);
    cam_pos = vglib.get_pos(camera);

    if (vaudio.is_playing(ambiance) == false) {
        vaudio.play_sound(ambiance);
    }
    if (vaudio.is_playing(qulaq) == false) {
        vaudio.play_sound(qulaq);
    }

    # --- 1. GROUND CHECK
    temp_ground_y = 0.0;
    through w :: walls -> loop {
        half = w[3] / 2.0;
        if (cam_pos[0] > w[0]-half && cam_pos[0] < w[0]+half && cam_pos[2] > w[2]-half && cam_pos[2] < w[2]+half) {
            top_y = w[1] + half;
            if (cam_pos[1] >= top_y - 0.5) { 
                temp_ground_y = top_y; 
            }
        }
    };

    # --- 2. TARGET HEIGHT TƏYİNİ ---
    current_speed = speed;
    if (vglib.key_down(vglib.LEFT_CTRL)) {
        target_h = temp_ground_y + crouch_height;
        current_speed = 0.05;
    } else {
        target_h = temp_ground_y + normal_height;
    }
    if (vglib.key_down(vglib.LEFT_SHIFT)) { 
        current_speed = current_speed * sprint_multiplier; 
    }

    # --- 3. JUMP & GRAVITY (İndi target_h artıq mövcuddur) ---
    if (vglib.key_down(vglib.SPACE) && is_grounded) {
        velocity_y = jump_force;
        is_grounded = false;
        current_y = current_y + 0.05; 
    }

    if (is_grounded == false) {
        velocity_y = velocity_y + gravity;
        current_y = current_y + velocity_y;

        if (velocity_y < 0.0 && current_y <= target_h) {
            current_y = target_h;
            velocity_y = 0.0;
            is_grounded = true;
        }
    } else {
        diff = target_h - current_y;
        if (vmath.abs(diff) > 0.001) {
            step = 0.2;
            if (diff < 0.0) { step = 0.1; }
            current_y = current_y + (diff * step);
        } else {
            current_y = target_h;
        }
        
        if (current_y > target_h + 0.5) {
            is_grounded = false;
        }
    }

    vglib.set_camera_height(camera, current_y);

    # --- NEXTBOT LOGIC ---
    if (is_dead == false) {
        ai_timer = ai_timer + 0.016;

        if (ai_timer >= ai_tick_rate) {
            res = vglib.pathfind(nextbot_pos, cam_pos, walls, 1.0); # grid_size yerinə 1.0 kifayətdir
            
            target_x = nextbot_pos[0] + (res[0] * nextbot_speed * 10.0);
            target_z = nextbot_pos[2] + (res[2] * nextbot_speed * 10.0);
            
            ai_timer = 0.0;
        }

        nextbot_pos[0] = nextbot_pos[0] + (target_x - nextbot_pos[0]) * 0.1;
        nextbot_pos[2] = nextbot_pos[2] + (target_z - nextbot_pos[2]) * 0.1;

        dist_3d = vglib.distance_3d(cam_pos, nextbot_pos);
        
        if (dist_3d < 40.0) {
            glitch_factor = 1.0 - (dist_3d / 40.0);
        } else { 
            glitch_factor = 0.0; 
        }

        if (dist_3d < 2.5) {
            is_dead = true;
            death_timer = 3.0;
            flash_timer = 0.5;
            vaudio.play_sound(death_sound);
            # Reset
            nextbot_pos = [60.0, 1.8, 60.0];
            target_x = 60.0;
            target_z = 60.0;
        }

        vaudio.sound_3d(qulaq, cam_pos, nextbot_pos, 80.0, 1.0);
    }

    # --- 5. SLIDING COLLISION (WASD) ---
    old_pos = vglib.get_pos(camera);
    moved = false;
    if (vglib.key_down(vglib.W)) { vglib.move_forward(camera, current_speed); moved = true; }
    if (vglib.key_down(vglib.S)) { vglib.move_forward(camera, current_speed * -1.0); moved = true; }
    if (vglib.key_down(vglib.A)) { vglib.move_right(camera, current_speed * -1.0); moved = true; }
    if (vglib.key_down(vglib.D)) { vglib.move_right(camera, current_speed); moved = true; }

    new_pos = vglib.get_pos(camera);

    if (moved || !is_grounded) {
        if (vglib.check_collision_map(new_pos, p_size, walls)) {
            if (velocity_y > 0.0) {
                test_y = [old_pos[0], new_pos[1], old_pos[2]];
                if (vglib.check_collision_map(test_y, p_size, walls)) {
                    velocity_y = 0.0;
                    current_y = old_pos[1];
                    vglib.set_pos(camera, old_pos[0], old_pos[1], old_pos[2]);
                }
            }
            test_x = [new_pos[0], old_pos[1], old_pos[2]];
            if (!vglib.check_collision_map(test_x, p_size, walls)) {
                vglib.set_pos(camera, test_x[0], old_pos[1], test_x[2]);
            } else {
                test_z = [old_pos[0], old_pos[1], new_pos[2]];
                if (!vglib.check_collision_map(test_z, p_size, walls)) {
                    vglib.set_pos(camera, test_z[0], old_pos[1], test_z[2]);
                } else {
                    vglib.set_pos(camera, old_pos[0], old_pos[1], old_pos[2]);
                }
            }
        }
    }

    # --- RENDER PIPELINE ---
    vglib.begin_texture_mode(screen_target);
        vglib.clear(vglib.rgba(128, 128, 140, 255));
        vglib.begin3d(camera);
            vglib.set_shader_camera(fog_shader, camera);
            vglib.begin_shader(fog_shader);
                vglib.plane_texture(ground_tex, 0.0, 0.0, 0.0, 500.0, 500.0);
                through w :: walls -> loop {
                    vglib.cube_texture(building_tex, w[0], w[1], w[2], w[3], vglib.WHITE);
                };
                vglib.billboard(camera, nextbot_tex, nextbot_pos, 4.0, vglib.WHITE);
            vglib.end_shader();
        vglib.end3d();
    vglib.end_texture_mode();

    vglib.begin_texture_mode(bodycam_target);
        vglib.clear(vglib.BLACK);
        vglib.set_shader_value(vhs_shader, "time", run_time);
        vglib.set_shader_value(vhs_shader, "noise_amount", glitch_factor); 
        vglib.set_shader_value(vhs_shader, "renderSize", [1920.0, 1080.0]);
        vglib.begin_shader(vhs_shader);
            vglib.draw_render_texture(screen_target);
        vglib.end_shader();
    vglib.end_texture_mode();

    vglib.begin();
        vglib.clear(vglib.BLACK);
        vglib.set_shader_value(vhs_color_shader, "time", run_time);
        vglib.set_shader_value(vhs_color_shader, "renderSize", [1920.0, 1080.0]);
        vglib.set_shader_value(vhs_color_shader, "offset", glitch_factor * 0.05);
        vglib.begin_shader(vhs_color_shader);
            vglib.draw_render_texture(bodycam_target);
        vglib.end_shader();

        if (is_dead == false && glitch_factor > 0.1) {
            red_alpha = int64(glitch_factor * 120.0);
            if (glitch_factor > 0.7) {
                red_alpha = red_alpha + int64(vmath.sin(run_time * 20.0) * 10.0);
            }
            if (glitch_factor > 0.8) {
                vignette_size = int64(vmath.sin(run_time * 25.0) * 50.0);
                vglib.rect(0, 0, 1920, vignette_size, vglib.BLACK); # Top bar
                vglib.rect(0, 1080 - vignette_size, 1920, vignette_size, vglib.BLACK);
            }
            vglib.rect(0, 0, 1920, 1080, vglib.rgba(150, 0, 0, red_alpha));
        }

        # Death & Glitch UI
         if (is_dead == true) {
            death_timer = death_timer - 0.008;
            nextbot_speed = 0.0;

            vglib.rect(0, 0, 1920, 1080, vglib.rgba(0, 0, 0, 255));

            if (flash_timer > 0.0) {
                flash_timer = flash_timer - 0.02; # Sürətlə azalır
                
                alpha = (flash_timer / 0.5) * 255.0;
                if (alpha < 0.0) { alpha = 0.0; }
                
                vglib.rect(0, 0, 1920, 1080, vglib.rgba(255, 255, 255, int64(alpha)));
            }

            vglib.rect(0, 0, 1920, 1080, vglib.rgba(180, 0, 0, (vmath.sin(run_time * 23.0) * 40.0 + 40.0)));

            vglib.rect(vmath.sin(run_time * 17.3) * 960.0 + 960.0, vmath.sin(run_time * 9.1)  * 540.0 + 270.0, 800.0, 6.0,  vglib.rgba(255, 0, 0, 180));
            vglib.rect(vmath.sin(run_time * 31.7) * 960.0 + 960.0, vmath.sin(run_time * 13.7) * 540.0 + 540.0, 500.0, 10.0, vglib.rgba(255, 255, 255, 120));
            vglib.rect(vmath.sin(run_time * 41.1) * 960.0 + 960.0, vmath.sin(run_time * 7.3)  * 540.0 + 810.0, 300.0, 4.0,  vglib.rgba(0, 255, 255, 90));

            vglib.text_ex(vcr_font, "SUBJECT LOST", (1920.0 - title_size[0]) / 2.0, 480, 80, vglib.RED);
            vglib.text_ex(vcr_font, "SYSTEM ERROR: SIGNAL CORRUPTED",(1920.0 - sub_size[0])   / 2.0, 580, 30, vglib.WHITE);

            if (death_timer <= 0.0) {
                is_dead = false;
                message = "";
                vglib.set_pos(camera, spawn_point[0], spawn_point[1], spawn_point[2]);
                nextbot_speed = 0.4;
            }

        }

        # Axon UI
        vglib.text_ex(vcr_font, "AXON BODY 3 - UNIT 402", 60, 60, 20, vglib.WHITE);
        if (vmath.sin(run_time * 4.0) > 0.0) { vglib.text_ex(vcr_font, "REC", 1800, 60, 25, vglib.RED); }
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vaudio.close_audio();
vglib.close();