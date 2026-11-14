use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };

app :: vraylib.App := vraylib.app(960, 540, "Vyne + managed raylib");
camera :: vraylib.Camera := app.orbit(vraylib.vec3(6.0, 4.0, 6.0), vraylib.vec3(0.0, 0.0, 0.0));
try {
    while app.frame() {
        camera.update();
        app.begin();
        app.clear(vraylib.RAYWHITE);
        camera.begin();
        vraylib.draw_cube(vraylib.vec3(0.0, 1.0, 0.0), 2.0, 2.0, 2.0, vraylib.BLUE);
        vraylib.draw_grid(20, 1.0);
        camera.end();
        vraylib.draw_text("Vyne / raylib / raymath / rlgl", 24, 24, 24, vraylib.DARKGRAY);
        app.end();
    }
} finally {
    app.close();
}
