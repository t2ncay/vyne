ruleset { dynamic_casting };

module vglib;

vglib.init(1920, 1080, "Vyne Ultra - Fullscreen Mode", vglib.FULLSCREEN);
camera = vglib.camera();

normal_height = 1.8;
crouch_height = 0.9;

vglib.disable_cursor();

rotation = 0.0;

while (vglib.running()) {
    vglib.update_camera(camera, vglib.CAMERA_FIRST_P);

    if (vglib.key_down(vglib.LEFT_SHIFT)) {
        vglib.set_camera_height(camera, crouch_height);
    } else {
        vglib.set_camera_height(camera, normal_height);
    }

    vglib.begin();
        vglib.clear(vglib.rgba(10, 10, 15, 255));
        
        vglib.begin3d(camera);
            vglib.grid(50, 1.0);
            vglib.cube(0.0, 1.0, 5.0, 2.0, rotation, vglib.CYAN);
        vglib.end3d();
        
        if (vglib.key_down(vglib.ESCAPE)) { vglib.enable_cursor(); }
    
    rotation = rotation + 5;

    vglib.text("VYNE ENGINE v0.1", 20, 20, 20, vglib.CYAN);
    vglib.text("FPS: 60", 20, 50, 20, vglib.CYAN);
    vglib.end();
}