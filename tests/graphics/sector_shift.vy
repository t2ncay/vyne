ruleset { dynamic_casting, warnings };
module vglib;
module vaudio;
module vmath;

# --- INITIALIZATION ---
vglib.init(1920, 1080, 60, "Vyne Pro - Sector 4 Incident", vglib.FULLSCREEN);
camera = vglib.camera();
vglib.set_pos(camera, 0.0, 1.8, -5.0);
vglib.disable_cursor();

# --- SHADERS & TEXTURES ---
fog_shader     = vglib.load_shader("tests/graphics/shaders/fog.vs", "tests/graphics/shaders/fog.fs");
vhs_shader     = vglib.load_shader("tests/graphics/shaders/vhs_horror.fs");
bodycam_shader = vglib.load_shader("tests/graphics/shaders/bodycam.fs");

building_tex = vglib.load_texture("tests/assets/building.jpg");
ground_tex   = vglib.load_texture("tests/assets/asphalt.jpg");
stalker_tex  = vglib.load_texture("tests/assets/stalker.png");

screen_target  = vglib.load_render_texture(1920, 1080);
bodycam_target = vglib.load_render_texture(1920, 1080);

# --- AUDIO SETUP ---
vaudio.init_audio();
ambiance = vaudio.load_sound("tests/assets/akira.wav");
radio_log = vaudio.load_sound("tests/assets/radio_log.wav");
vaudio.play_sound(ambiance);

# --- GAME STATE ---
run_time = 0.0;
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
    walls = walls + [[-25.0, 20.0, z_pos, 40.0]];
    walls = walls + [[25.0, 20.0, z_pos, 40.0]];
};

final_z = 500.0;
walls = walls + [[0.0, 50.0, final_z, 100.0]];

fade_alpha = 0.0;
final_triggered = false;

while (vglib.running()) {
    run_time = run_time + 0.016;
    cam_pos = vglib.get_pos(camera);
    is_moving = false;
    
    # Speed & Sprint
    current_speed = base_speed;
    if (vglib.key_down(vglib.LEFT_SHIFT)) { current_speed = base_speed * sprint_multiplier; }

    dist_to_stalker = vmath.abs(cam_pos[2] - stalker_z);
    glitch_val = 0.0;
    if (dist_to_stalker < 35.0) {
        glitch_val = (35.0 - dist_to_stalker) / 12.0; 
        if (dist_to_stalker < 70.0) { stalker_z = stalker_z + 180.0; }
    }

    # --- FINAL TRIGGER LOGIC ---
    if (cam_pos[2] > 350.0) {
        final_triggered = true;
        if (fade_alpha < 1.0) { 
            fade_alpha = fade_alpha + 0.005; 
            
            # AUDIO FADE-OUT: Akira musiqisi tədricən sönür
            vaudio.sound_volume(ambiance, 1.0 - fade_alpha);
        }
        # Finalda hərəkəti dondurmaq üçün sürəti öldürürük
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
        fog_density = 0.1 + (vmath.sin(run_time * 2.0) * 0.05);
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
        
        vglib.begin_shader(vhs_shader);
            vglib.draw_render_texture(screen_target);
        vglib.end_shader();
    vglib.end_texture_mode();

    vglib.begin();
        vglib.clear(vglib.BLACK);
        vglib.set_shader_value(bodycam_shader, "time", run_time);
        vglib.begin_shader(bodycam_shader);
            vglib.draw_render_texture(bodycam_target);
        vglib.end_shader();

        vglib.text("AXON BODY 3 | UNIT 402", 60, 60, 22, vglib.WHITE);
        vglib.text("LAT: 40.4093 N | LON: 49.8671 E", 60, 90, 16, vglib.GRAY);
        vglib.text(message, 60, 1000, 25, vglib.RED);
        if (vmath.sin(run_time * 4.0) > 0.0) { vglib.text("● REC", 1750, 60, 30, vglib.RED); }

        if (final_triggered) {
            alpha = int64(fade_alpha * 255.0);
            vglib.rect(0, 0, 1920, 1080, vglib.rgba(0, 0, 0, alpha));
            if (fade_alpha >= 0.95) { vglib.text("YOU NEVER EXISTED", 700, 500, 50, vglib.RED); }
        }

        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end(); 
}

vaudio.close_audio();
vglib.close();