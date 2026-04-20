ruleset { dynamic_casting };
module vglib;

vglib.init(1920, 1080, 100, "Vyne Pro Engine", vglib.FULLSCREEN);
camera = vglib.camera();
vglib.disable_cursor();

mouse_sens = 0.15;
speed = 0.15;
normal_height = 1.8;
crouch_height = 0.9;

current_y = normal_height;
velocity_y = 0.0;
gravity = -0.012;
jump_force = 0.35;
is_grounded = true;

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
        vglib.clear(vglib.rgba(10, 10, 15, 255));
        
        vglib.begin3d(camera);
            vglib.grid(50, 1.0);
            vglib.cube(0.0, 1.0, 5.0, 2.0, 0.0, vglib.CYAN);
        vglib.end3d();

        vglib.text("VYNE PRO ENGINE v0.1", 20, 20, 20, vglib.CYAN);
        vglib.text("Y-POS: " + string(current_y), 20, 50, 20, vglib.WHITE);
        vglib.text("FPS: " + string(vglib.get_fps()), 20, 80, 20, vglib.GREEN);
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    vglib.end();
}

vglib.close();