use "../engine/collision.vy";
use "../engine/input.vy";
use native vcore;
use "../ui/theme.vy";

fn vmge_viewer() -> Int64 {
    app :: vraylib.App := vraylib.App(0);
    frames :: Int64 := 0;
    try {
        project :: String := vmge_project_root();
        map_path :: String := vmge_option("VYNE_VMGE_MAP", "tau_map.dat");
        # The portable scene is used only when no explicit map was requested.
        if vraylib.environment("VYNE_VMGE_MAP") == null && !vraylib.file_exists(map_path) { map_path = project + "/maps/demo.dat"; }
        world :: VMGEMap := vmge_map_load(map_path, vmge_setting("VYNE_VMGE_CAPACITY", 16384), false);
        app = vmge_window("VMGE - Bodycam Map Viewer");
        se_theme_load(app,project);
        assets :: String := vmge_asset_root(project);
        textures :: Array := vmge_palette(app, assets);
        models :: Array := vmge_models(app, textures);
        ground_texture :: vraylib.Texture := textures[4];
        if assets != "" { ground_texture = app.load_texture("Dirt/Dirt_20-512x512.png"); }
        ground :: vraylib.Model := app.plane(ground_texture, 200.0, 200.0);
        player :: VMGEPlayer := vmge_player(app);
        camera :: vraylib.Camera := player.camera;
        app.assets(project);
        fog :: vraylib.Shader := app.load_shader("shaders/fog.vs", "shaders/fog.fs");
        vhs :: vraylib.Shader := app.load_shader(null, "shaders/vhs.fs");
        scanline :: vraylib.Shader := app.load_shader(null, "shaders/scanline.fs");
        through model :: models -> loop { model.set_shader(fog); };
        ground.set_shader(fog);
        scene :: vraylib.RenderTarget := app.screen_target();
        bodycam :: vraylib.RenderTarget := app.screen_target();
        if vmge_setting("VYNE_VMGE_AUDIO", 1) != 0 && assets != "" {
            app.assets(assets);
            music :: vraylib.Music := app.music("akira.wav", 1.0);
            out("VMGE_AUDIO active=", music.valid());
        }
        captured :: Bool := vmge_setting("VYNE_VMGE_HIDDEN", 0) == 0;
        app.capture_mouse(captured);
        automated :: Bool := vmge_setting("VYNE_VMGE_AUTOTEST", 0) != 0;
        while app.frame() {
            captured = vmge_capture(app, captured);
            if captured { camera.look(); }
            forward :: Float64 := vmge_forward();
            right :: Float64 := vmge_right();
            jump :: Bool := vraylib.is_key_pressed(vraylib.KEY_SPACE);
            crouch :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL);
            if automated { forward = 0.1; jump = app.frames() == 2; crouch = app.frames() == 1; }
            vmge_player_update(player, world, forward, right, vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT), crouch, jump, app.dt());
            if vraylib.is_key_pressed(vraylib.KEY_J) { player.reset(); }
            fog.set("cameraPos", camera.position());
            scene.begin();
            app.clear(vraylib.rgba(128, 128, 140, 255));
            camera.begin();
            ground.draw(vraylib.vec3(0.0, 0.0, 0.0), 1.0, vraylib.WHITE);
            vmge_draw_map(world, models, camera.position(), 100.0);
            camera.end();
            scene.end();
            vhs.set("time", app.elapsed());
            bodycam.begin();
            app.clear(vraylib.BLACK);
            vhs.begin();
            scene.present(float64(bodycam.width()), float64(bodycam.height()));
            vhs.end();
            bodycam.end();
            scanline.set("renderSize", vraylib.vec2(float64(app.width()), float64(app.height())));
            app.begin();
            app.clear(vraylib.BLACK);
            scanline.begin();
            bodycam.present(float64(app.width()), float64(app.height()));
            scanline.end();
            se_draw_heading("AXON BODY 3 - VMGE", 24, 24, 20, vraylib.WHITE);
            se_draw_text(vcore.now(), 24, 52, 18, vraylib.WHITE);
            se_draw_heading("REC", app.width() - 90, 24, 24, vraylib.RED);
            se_draw_text("WASD move | Shift sprint | Ctrl crouch | Space jump", 24, app.height() - 48, 14, vraylib.WHITE);
            se_draw_text("J reset | Tab mouse | F11 fullscreen | Escape exit", 24, app.height() - 28, 14, vraylib.WHITE);
            app.end();
        }
        frames = app.frames();
        out("VMGE_VIEWER_STATE cubes=", world.count(), " uniforms=", app.uniform_queries());
    } finally { app.close(); }
    out("VMGE_VIEWER_COMPLETE frames=", frames, " live_handles=", vraylib.live_handles());
    return frames;
}
