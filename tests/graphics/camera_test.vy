ruleset { dynamic_casting };
module vglib;
module vaudio;

vglib.init(1920, 1080, 100, "Vyne Pro Engine - Mist Atmosphere", vglib.FULLSCREEN);
camera = vglib.camera();
vglib.disable_cursor();

fog_shader = vglib.load_shader("tests/graphics/shaders/fog.fs");

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
    vglib.rotate_view(camera, mouse_sens);

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

    if (vglib.key_down(vglib.W)) { vglib.move_forward(camera, current_speed); }
    if (vglib.key_down(vglib.S)) { vglib.move_forward(camera, current_speed * -1.0); }
    if (vglib.key_down(vglib.A)) { vglib.move_right(camera, current_speed * -1.0); }
    if (vglib.key_down(vglib.D)) { vglib.move_right(camera, current_speed); }

    vglib.begin();
        vglib.clear(vglib.rgba(128, 128, 140, 255));
        
        vglib.begin3d(camera);
            vglib.set_shader_camera(fog_shader, camera);
            
            vglib.begin_shader(fog_shader);
                vglib.grid(100, 1.0);
                vglib.cube(0.0, 2.5, 10.0, 5.0, 0.0, vglib.CYAN);
                
                vglib.cube(10.0, 2.5, 20.0, 5.0, 0.0, vglib.rgba(40, 20, 20, 255));
                vglib.cube(-15.0, 2.5, 35.0, 5.0, 0.0, vglib.rgba(20, 40, 20, 255));
            vglib.end_shader();
            
        vglib.end3d();

        vglib.text("VYNE PRO - GRAY MIST", 20, 20, 20, vglib.BLACK);
        vglib.text("FPS: " + string(vglib.get_fps()), 20, 80, 20, vglib.BLACK);
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vglib.close();