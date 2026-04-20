ruleset { dynamic_casting };

module vglib;

vglib.init(1920, 1080, "Vyne Ultra - Fullscreen Mode", vglib.FULLSCREEN);
camera = vglib.camera();

vglib.disable_cursor();

while (vglib.running()) {
    vglib.update_camera(camera, vglib.CAMERA_FIRST_P);

    vglib.begin();
        vglib.clear(vglib.rgba(10, 10, 15, 255));
        
        vglib.begin3d(camera);
            vglib.grid(50, 1.0);
            vglib.cube(0.0, 1.0, 0.0, 2.0, 0.0, vglib.CYAN);
        vglib.end3d();
        
        if (vglib.key_down(vglib.ESCAPE)) {
            vglib.enable_cursor();
        }
    vglib.end();
}

vglib.close();