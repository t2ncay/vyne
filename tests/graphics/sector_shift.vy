ruleset { dynamic_casting, warnings };
module vglib;
module vaudio;
module vmath;

vglib.init(1920, 1080, 60, "Vyne Pro - Sector 4 Incident", vglib.FULLSCREEN);
camera = vglib.camera();
vglib.set_pos(camera, 0.0, 1.8, -5.0);
vglib.disable_cursor();

fog_shader     = vglib.load_shader("tests/graphics/shaders/fog.vs", "tests/graphics/shaders/fog.fs");
vhs_shader     = vglib.load_shader("tests/graphics/shaders/vhs_horror.fs");
bodycam_shader = vglib.load_shader("tests/graphics/shaders/bodycam.fs");

building_tex = vglib.load_texture("tests/assets/building.jpg");
ground_tex   = vglib.load_texture("tests/assets/asphalt.jpg");

screen_target  = vglib.load_render_texture(1920, 1080);
bodycam_target = vglib.load_render_texture(1920, 1080);

vaudio.init_audio();
ambiance = vaudio.load_sound("tests/assets/akira.wav");
vaudio.play_sound(ambiance);

run_time = 0.0;
game_event = 0;
fog_density = 0.05;
player_speed = 0.12;
message = "OBJECTIVE: PATROL THE CORRIDOR";

walls = [];
through i :: 0..100 -> loop {
    z_pos = i * 25.0;
    walls = walls + [[-25.0, 20.0, z_pos, 40.0]];
    walls = walls + [[25.0, 20.0, z_pos, 40.0]];
};

final_z = 2500.0;
walls = walls + [[0.0, 50.0, final_z, 100.0]];

fade_alpha = 0.0;
final_triggered = false;

while (vglib.running()) {
    run_time = run_time + 0.016;
    cam_pos = vglib.get_pos(camera);

    if (cam_pos[2] > 380.0) {
        final_triggered = true;
        if (fade_alpha < 1.0) {
            fade_alpha = fade_alpha + 0.005; 
        }
    }
    
    if cam_pos[2] > 50.0 && game_event == 0 {
        game_event = 1;
        message = "WARNING: HIGH FOG CONCENTRATION";
    }
    
    if cam_pos[2] > 150.0 && game_event == 1 {
        game_event = 2;
        message = "SYSTEM ERROR: BODYCAM MALFUNCTION";
        vaudio.set_dsp(0.8, 1); # Distortion artır (Əgər DSP kodun aktivdirsə)
    }

    if game_event == 2 {
        fog_density = 0.1 + (vmath.sin(run_time * 2.0) * 0.05);
    }

    vglib.rotate_view(camera, 0.15);
    
    move_vec = 0.0;
    if (vglib.key_down(vglib.W)) { 
        vglib.move_forward(camera, player_speed); 
        through w :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), [0.8, 1.8, 0.8], w, w[3])) {
                vglib.move_forward(camera, -player_speed);
            }
        };
    }

    if (vglib.key_down(vglib.S)) { 
        vglib.move_forward(camera, player_speed * -1.0); 
        through wall :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), [0.8, 1.8, 0.8], wall, wall[3])) {
                vglib.move_forward(camera, player_speed);
            }
        };
    }
    if (vglib.key_down(vglib.A)) { 
        vglib.move_right(camera, player_speed * -1.0); 
        through wall :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), [0.8, 1.8, 0.8], wall, wall[3])) {
                vglib.move_right(camera, player_speed);
            }
        };
    }
    if (vglib.key_down(vglib.D)) { 
        vglib.move_right(camera, player_speed); 
        through wall :: walls -> loop {
            if (vglib.check_collision(vglib.get_pos(camera), [0.8, 1.8, 0.8], wall, wall[3])) {
                vglib.move_right(camera, -player_speed);
            }
        };
    }

    vglib.begin_texture_mode(screen_target);
        vglib.clear(vglib.rgba(10, 20, 45, 255)); # Qatı mavi
        vglib.begin3d(camera);
            vglib.set_shader_camera(fog_shader, camera);
            vglib.set_shader_value(fog_shader, "u_density", fog_density);
            vglib.begin_shader(fog_shader);
                vglib.plane_texture(ground_tex, 0.0, 0.0, final_z / 2.0, 100.0, 3000.0);
                through w :: walls -> loop {
                    vglib.cube_texture(building_tex, w[0], w[1], w[2], w[3], vglib.WHITE);
                };
            vglib.end_shader();
        vglib.end3d();
    vglib.end_texture_mode();

    vglib.begin_texture_mode(bodycam_target);
        vglib.clear(vglib.BLACK);
        vhs_time = run_time;
        if game_event == 2 { vhs_time = run_time * 5.0; } 
        
        vglib.set_shader_value(vhs_shader, "time", vhs_time);
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
        
        if (vmath.sin(run_time * 4.0) > 0.0) {
            vglib.text("● REC", 1750, 60, 30, vglib.RED);
        }

        if (final_triggered) {
            alpha_val = int64(fade_alpha * 255.0);
            black_overlay = vglib.rgba(0, 0, 0, alpha_val);
            vglib.rect(0, 0, 1920, 1080, black_overlay);

            if (fade_alpha >= 1.0) {
                vglib.text("YOU NEVER EXISTED", 700, 500, 50, vglib.RED);
            }
        }

        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    
    vglib.end(); 
}

vaudio.close_audio();
vglib.close();