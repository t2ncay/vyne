use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };

vraylib.open(960, 540, "Vyne + raylib 6.2", 60);
camera :: Int64 := vraylib.camera_create(vraylib.vec3(6.0, 4.0, 6.0), vraylib.vec3(0.0, 0.0, 0.0), 60.0);
while !vraylib.window_should_close() {
    region frame {
        vraylib.begin_drawing();
        vraylib.clear_background(vraylib.RAYWHITE);
        vraylib.begin_mode_3d(vraylib.camera_view(camera));
        vraylib.draw_cube(vraylib.vec3(0.0, 1.0, 0.0), 2.0, 2.0, 2.0, vraylib.BLUE);
        vraylib.draw_grid(20, 1.0);
        vraylib.end_mode_3d();
        vraylib.draw_text("Vyne / raylib / raymath / rlgl", 24, 24, 24, vraylib.DARKGRAY);
        vraylib.end_drawing();
    };
}
vraylib.shutdown();
