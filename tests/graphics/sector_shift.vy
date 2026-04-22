ruleset { dynamic_casting, warnings };
module vglib;
module vaudio;
module vmath;

# --- INITIALIZATION ---
vglib.init(1920, 1080, 60, "Vyne Pro - Sector 4 Incident", vglib.FULLSCREEN);
camera = vglib.camera(90.0);
vglib.set_pos(camera, 0.0, 1.8, -5.0);
vglib.disable_cursor();

# --- SHADERS & TEXTURES ---
fog_shader      = vglib.load_shader("tests/graphics/shaders/fog.vs", "tests/graphics/shaders/fog.fs");
vhs_shader      = vglib.load_shader("tests/graphics/shaders/vhs.fs");
scanline_shader = vglib.load_shader("tests/graphics/shaders/scanline.fs");

building_tex = vglib.load_texture("tests/assets/wall.jpeg");
ground_tex   = vglib.load_texture("tests/assets/asphalt_road_3.jpg");
stalker_tex  = vglib.load_texture("tests/assets/yusif.jpeg");

vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

screen_target  = vglib.load_render_texture(1920, 1080);
bodycam_target = vglib.load_render_texture(1920, 1080);

# --- AUDIO SETUP ---
vaudio.init_audio();
ambiance = vaudio.load_sound("tests/assets/akira.wav");
radio_log = vaudio.load_sound("tests/assets/radio_log.wav");
vaudio.sound_volume(ambiance, 0.5);
vaudio.sound_volume(radio_log, 1.5);
vaudio.play_sound(ambiance);

# --- GAME STATE ---
run_time = 0.0;
event_active = false;
event_start_time = 0.0;
event_text = "THE WALLS ARE MOVING...";
game_event = 0;
fog_density = 0.05;
base_speed = 0.12;
sprint_multiplier = 2.5;
message = "OBJECTIVE: PATROL THE CORRIDOR";
radio_played = false;

# Stalker Logic
stalker_z = 120.0;
glitch_val = 0.0;

# Map Design
walls = [];
through i :: 0..25 -> loop {
    z_pos = i * 20.0; 
    
    # PARAMETRLƏR:
    # w[0] = X (sağ/sol)
    # w[1] = Y (hündürlük koordinatı)
    # w[2] = Z (dərinlik koordinatı)
    # w[3] = Ölçü (burada divarın hündürlüyü və eni kimi gedir)
    
    wall_height = 20.0;
    
    walls.push([-15.0, 10.0, z_pos, wall_height]);
    walls.push([15.0, 10.0, z_pos, wall_height]);
};

final_z = 500.0;
walls = walls + [[0.0, 50.0, final_z, 100.0]];

fade_alpha = 0.0;
final_triggered = false;

freeze = false;

while (vglib.running()) {
    run_time = run_time + 0.016;
    cam_pos = vglib.get_pos(camera);
    is_moving = false;

    if (event_active) {
        t = run_time - event_start_time;

        if (t < 3.0) {
            freeze = true;
        } else {
            freeze = false;
            event_active = false;
        }
    } else {
        freeze = false;
    }
    
    # Speed & Sprint
    current_speed = base_speed;
    if (vglib.key_down(vglib.LEFT_SHIFT)) { current_speed = base_speed * sprint_multiplier; }

    dist_to_stalker = vmath.abs(cam_pos[2] - stalker_z);
    glitch_val = 0.0;
    if (vmath.abs(cam_pos[2] - stalker_z) < 5.0 && event_active == false) {
        event_active = true;
        event_start_time = run_time; 
        if (dist_to_stalker < 70.0) { stalker_z = stalker_z + 180.0; }
    }

    # --- FINAL TRIGGER LOGIC ---
    if (cam_pos[2] > 350.0) {
        final_triggered = true;
        if (fade_alpha < 1.0) { 
            fade_alpha = fade_alpha + 0.005; 
            
            vaudio.sound_volume(ambiance, 0.5 - fade_alpha);
            vaudio.sound_volume(radio_log, 1.5 - fade_alpha);
        }
        if (fade_alpha > 0.9) { current_speed = 0.0; }
    }
    
    # --- ENVIRONMENTAL STORYTELLING ---
    if (cam_pos[2] > 100.0 && radio_played == false) {
        vaudio.play_sound(radio_log);
        radio_played = true;
        message = "INCOMING RADIO SIGNAL...";
    }

    if (cam_pos[2] > 50.0 && game_event == 0) {
        game_event = 1;
        message = "WARNING: HIGH FOG CONCENTRATION";
    }
    
    if (cam_pos[2] > 150.0 && game_event == 1) {
        game_event = 2;
        message = "SYSTEM ERROR: BODYCAM MALFUNCTION";
        vaudio.set_dsp(0.8, 1);
    }

    if (game_event == 2) {
        fog_density = 28 + (vmath.sin(run_time * 5.0) * 0.1);
    }

    vglib.rotate_view(camera, 0.15);
    
    # --- MOVEMENT (W, S, A, D) ---
    if (vglib.key_down(vglib.W)) { 
        vglib.move_forward(camera, current_speed); 
        through w :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), [0.8, 1.8, 0.8], w, w[3])) { vglib.move_forward(camera, -current_speed); }
        };
        is_moving = true;
    }
    if (vglib.key_down(vglib.S)) { 
        vglib.move_forward(camera, -current_speed); 
        through w :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), [0.8, 1.8, 0.8], w, w[3])) { vglib.move_forward(camera, current_speed); }
        };
        is_moving = true;
    }
    if (vglib.key_down(vglib.A)) { 
        vglib.move_right(camera, -current_speed); 
        through w :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), [0.8, 1.8, 0.8], w, w[3])) { vglib.move_right(camera, current_speed); }
        };
        is_moving = true;
    }
    if (vglib.key_down(vglib.D)) { 
        vglib.move_right(camera, current_speed); 
        through w :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), [0.8, 1.8, 0.8], w, w[3])) { vglib.move_right(camera, -current_speed); }
        };
        is_moving = true;
    }

    # Head Bob Logic
    if (is_moving) {
        bob_f = 10.0; bob_a = 0.05;
        if (vglib.key_down(vglib.LEFT_SHIFT)) { bob_f = 16.0; bob_a = 0.12; }
        bob = vmath.sin(run_time * bob_f) * bob_a;
        vglib.set_camera_height(camera, 1.8 + bob);
    } else {
        vglib.set_camera_height(camera, 1.8);
    }

    # --- RENDER PASSES ---
    vglib.begin_texture_mode(screen_target);
        vglib.clear(vglib.rgba(10, 20, 45, 255));
        vglib.begin3d(camera);
            vglib.set_shader_camera(fog_shader, camera);
            vglib.set_shader_value(fog_shader, "u_density", fog_density);
            
            vglib.begin_shader(fog_shader);
                vglib.plane_texture(ground_tex, 0.0, 0.0, final_z / 2.0, 100.0, 700.0);
                
                through w :: walls -> loop {
                    vglib.cube_texture(building_tex, w[0], w[1], w[2], w[3], vglib.WHITE);
                };
                vglib.billboard(camera, stalker_tex, [2.0, 1.5, stalker_z], 3.0);
                
            vglib.end_shader();
        vglib.end3d();
    vglib.end_texture_mode();

    vglib.begin_texture_mode(bodycam_target);
        vglib.clear(vglib.BLACK);
        vt = run_time;
        if game_event == 2 { vt = run_time * 5.0; } 
        
        vglib.set_shader_value(vhs_shader, "time", vt);
        vglib.set_shader_value(vhs_shader, "noise_amount", glitch_val); 
        vglib.set_shader_value(vhs_shader, "renderSize", [1920.0, 1080.0]);
        
        vglib.begin_shader(vhs_shader);
            vglib.draw_render_texture(screen_target);
        vglib.end_shader();
    vglib.end_texture_mode();

    vglib.begin();
        vglib.clear(vglib.BLACK);
        vglib.set_shader_value(scanline_shader, "time", run_time);
        vglib.set_shader_value(scanline_shader, "renderSize", [1920.0, 1080.0]);
        vglib.begin_shader(scanline_shader);
            vglib.draw_render_texture(bodycam_target);
        vglib.end_shader();

        vglib.text_ex(vcr_font, "AXON BODY 3 | UNIT 402", 60, 60, 24, vglib.WHITE);
        vglib.text_ex(vcr_font, "LAT: 40.4093 N | LON: 49.8671 E", 60, 90, 16, vglib.GRAY);
        vglib.text_ex(vcr_font, message, 60, 1000, 25, vglib.RED);
        if (vmath.sin(run_time * 4.0) > 0.0) { vglib.text_ex(vcr_font,"REC", 1750, 60, 30, vglib.RED); }

        if (final_triggered) {
            alpha = int64(fade_alpha * 255.0);
            vglib.rect(0, 0, 1920, 1080, vglib.rgba(0, 0, 0, alpha));
            if (fade_alpha >= 0.95) { vglib.text_ex(vcr_font,"YOU NEVER EXISTED", 700, 450, 50, vglib.RED); }
        }

        if (freeze) {
            vglib.rect(0, 0, 1920, 1080, vglib.BLACK);

            shake = vmath.sin(run_time * 60.0) * 2.0;
            
            if (vmath.sin(run_time * 25.0) > 0.0) {
                vglib.text_ex(vcr_font, event_text, int64(650 + shake), int64(520 + shake), 45, vglib.RED);
            }
            
            if (vmath.sin(run_time * 100.0) > 0.8) {
                vglib.rect(0, int64(vmath.sin(run_time) * 1080.0), 1920, 2, vglib.GRAY);
            }
        }

        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end(); 
}

vaudio.close_audio();
vglib.close();