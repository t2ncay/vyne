# Vyne Pro - Bodycam Horror. Managed external vraylib facade.
# Run from the repository root: vynec --run examples/graphics/camera_test.vy
# WASD: move; mouse: look; Shift: sprint; Ctrl: crouch; Space: jump.
# Tab: capture/release mouse; F11: fullscreen; Escape: exit.
use external "vraylib/vraylib.vy";
use native vmath;
ruleset { dynamic_casting };

fn setting(name :: String, fallback :: Int64) -> Int64 {
    value := vraylib.environment(name);
    if value == null { return fallback; }
    return int64(value);
}

fn asset_root() -> String {
    roots := ["examples/assets", vraylib.get_application_directory() + "../assets", vraylib.get_application_directory() + "../examples/assets"];
    through root :: roots -> loop {
        if vraylib.file_exists(root + "/building.jpg") { return root; }
    };
    throw "Bodycam asset directory not found";
}

# Axis-separated horizontal collision preserves sliding along building walls.
fn walk(camera :: vraylib.Camera, forward :: Float64, right :: Float64, walls :: Array, height :: Float64) -> Bool {
    previous :: vraylib.Vector3 := camera.position();
    camera.move(forward, right);
    candidate :: vraylib.Vector3 := camera.position();
    blocked_x :: Bool := false;
    blocked_z :: Bool := false;
    through wall :: walls -> loop {
        box :: vraylib.BoundingBox := vraylib.cube_box(wall[0], wall[1], wall[2], wall[3]);
        probe_x := vraylib.player_box(vraylib.vec3(candidate.x, previous.y, previous.z), 0.8, height);
        probe_z := vraylib.player_box(vraylib.vec3(previous.x, previous.y, candidate.z), 0.8, height);
        if vraylib.check_collision_boxes(probe_x, box) { blocked_x = true; }
        if vraylib.check_collision_boxes(probe_z, box) { blocked_z = true; }
    };
    if blocked_x { candidate.x = previous.x; }
    if blocked_z { candidate.z = previous.z; }
    camera.set_position(candidate);
    return true;
}

width :: Int64 := setting("VYNE_CAMERA_WIDTH", 1920);
height :: Int64 := setting("VYNE_CAMERA_HEIGHT", 1080);
frame_limit :: Int64 := setting("VYNE_CAMERA_FRAMES", 0);
frames :: Int64 := 0;
elapsed :: Float64 := 0.0;
eye_y :: Float64 := 1.8;
velocity_y :: Float64 := 0.0;
grounded :: Bool := true;
mouse_captured :: Bool := true;
air_x :: Float64 := -330.0;
air_y :: Float64 := 100.0;
cat_x :: Float64 := 35.0;
app :: vraylib.App := vraylib.App(0);

walls := [
    [-7.0, 5.0, 10.0, 10.0], [-7.0, 8.0, 22.0, 16.0],
    [-10.0, 5.0, 35.0, 10.0], [-12.0, 12.0, 50.0, 24.0],
    [7.0, 5.0, 12.0, 10.0], [9.0, 7.0, 25.0, 14.0],
    [7.0, 5.0, 38.0, 10.0], [15.0, 15.0, 55.0, 30.0],
    [0.0, 5.0, -10.0, 15.0], [25.0, 5.0, 40.0, 10.0],
    [35.0, 8.0, 40.0, 16.0], [45.0, 5.0, 30.0, 10.0],
    [0.0, 25.0, 90.0, 50.0], [-25.0, 2.5, 20.0, 5.0],
    [20.0, 2.5, 10.0, 5.0], [-30.0, 10.0, 80.0, 20.0]
];

# The application owns assets, passes, streamed audio and frame memory.
try {
    app = vraylib.app(width, height, "Vyne Pro - Bodycam Horror");
    app.fps(75);
    app.limit(frame_limit);
    app.assets(asset_root());
    camera :: vraylib.Camera := app.camera(vraylib.vec3(30.0, 1.8, 0.0), vraylib.vec3(30.0, 1.8, 1.0), 80.0);
    app.capture_mouse(true);

    fog :: vraylib.Shader := app.load_shader("../graphics/shaders/fog.vs", "../graphics/shaders/fog.fs");
    vhs :: vraylib.Shader := app.load_shader(null, "../graphics/shaders/vhs.fs");
    scanline :: vraylib.Shader := app.load_shader(null, "../graphics/shaders/scanline.fs");
    font :: vraylib.Font := app.load_font("VCR_OSD_MONO_1.001.ttf");
    building_texture :: vraylib.Texture := app.load_texture("building.jpg");
    ground_texture :: vraylib.Texture := app.load_texture("asphalt_road_3.jpg");
    building :: vraylib.Model := app.cube(building_texture);
    ground :: vraylib.Model := app.plane(ground_texture, 200.0, 200.0);
    cat :: vraylib.Model := app.load_model("12222_Cat_v1_l3.obj");
    cat_texture :: vraylib.Texture := app.load_texture("Cat_diffuse.jpg");
    cat.set_texture(cat_texture, 0);
    airplane :: vraylib.Model := app.load_model("11803_Airplane_v1_l1.obj");
    airplane_textures := [
        app.load_texture("11803_Airplane_body_diff.jpg"),
        app.load_texture("11803_Airplane_tail_diff.jpg"),
        app.load_texture("11803_Airplane_wing_big_L_diff.jpg"),
        app.load_texture("11803_Airplane_wing_big_R_diff.jpg")
    ];
    material_count :: Int64 := airplane.materials();
    through i :: 0..3 -> loop {
        if i < material_count { airplane.set_texture(airplane_textures[i], i); }
    };
    building.set_shader(fog);
    ground.set_shader(fog);
    cat.set_shader(fog);
    airplane.set_shader(fog);

    scene :: vraylib.RenderTarget := app.screen_target();
    bodycam :: vraylib.RenderTarget := app.screen_target();
    if setting("VYNE_CAMERA_AUDIO", 1) != 0 {
        music :: vraylib.Music := app.music("akira.wav", 1.0);
        out("BODYCAM_AUDIO active=", music.valid());
    }

    while app.frame() {
            dt :: Float64 := app.dt();
            elapsed = elapsed + dt;
            if vraylib.is_key_pressed(vraylib.KEY_F11) { vraylib.toggle_fullscreen(); }
            if vraylib.is_key_pressed(vraylib.KEY_TAB) {
                if mouse_captured {
                    mouse_captured = false;
                    app.capture_mouse(false);
                } else {
                    mouse_captured = true;
                    app.capture_mouse(true);
                }
            }
            if mouse_captured { camera.look(); }
            speed :: Float64 := 9.0;
            player_height :: Float64 := 1.8;
            if vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL) { speed = 3.0; player_height = 0.9; }
            if vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT) { speed = speed * 2.5; }
            forward :: Float64 := 0.0;
            right :: Float64 := 0.0;
            if vraylib.is_key_down(vraylib.KEY_W) { forward = forward + 1.0; }
            if vraylib.is_key_down(vraylib.KEY_S) { forward = forward - 1.0; }
            if vraylib.is_key_down(vraylib.KEY_D) { right = right + 1.0; }
            if vraylib.is_key_down(vraylib.KEY_A) { right = right - 1.0; }
            if forward != 0.0 && right != 0.0 { forward = forward * 0.70710678; right = right * 0.70710678; }
            walk(camera, forward * speed * dt, right * speed * dt, walls, player_height);

            position :: vraylib.Vector3 := camera.position();
            floor_y :: Float64 := 0.0;
            through wall :: walls -> loop {
                half :: Float64 := wall[3] * 0.5;
                top :: Float64 := wall[1] + half;
                if position.x > wall[0] - half && position.x < wall[0] + half && position.z > wall[2] - half && position.z < wall[2] + half && eye_y - player_height >= top - 0.15 {
                    floor_y = vmath.max(floor_y, top);
                }
            };
            target_eye :: Float64 := floor_y + player_height;
            if vraylib.is_key_pressed(vraylib.KEY_SPACE) && grounded { velocity_y = 21.0; grounded = false; }
            if eye_y > target_eye + 0.5 { grounded = false; }
            if !grounded {
                velocity_y = velocity_y - 43.2 * dt;
                eye_y = eye_y + velocity_y * dt;
                if eye_y <= target_eye { eye_y = target_eye; velocity_y = 0.0; grounded = true; }
            } else {
                eye_y = eye_y + (target_eye - eye_y) * (1.0 - vmath.exp(-10.0 * dt));
            }
            camera.set_height(eye_y);
            position = camera.position();
            cat_x = cat_x - elapsed * 0.6 * dt;
            air_x = air_x + 48.0 * dt;
            air_y = air_y - 4.8 * dt;

            fog.set("cameraPos", position);
            scene.begin();
            app.clear(vraylib.rgba(128, 128, 140, 255));
            camera.begin();
            ground.draw_scaled(vraylib.vec3(0.0, 0.0, 0.0), vraylib.vec3(1.0, 1.0, 1.0), vraylib.WHITE);
            through wall :: walls -> loop {
                building.draw_scaled(vraylib.vec3(wall[0], wall[1], wall[2]), vraylib.vec3(wall[3], wall[3], wall[3]), vraylib.WHITE);
            };
            cat.draw(vraylib.vec3(cat_x, 2.5 + vmath.sin(elapsed * 2.0) * 0.2, 25.0), 0.05, vraylib.WHITE);
            airplane.draw(vraylib.vec3(air_x, air_y, -50.0), 0.02, vraylib.WHITE);
            camera.end();
            scene.end();

            vhs.set("time", elapsed);
            # vhs.fs currently declares renderSize without using it, so it is
            # optimized out. The managed shader API diagnoses missing uniforms.
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
            font.draw("AXON BODY 3 - UNIT 402", vraylib.vec2(60.0, 60.0), 20.0, vraylib.WHITE);
            font.draw("2026-04-21 01:14:23", vraylib.vec2(60.0, 90.0), 18.0, vraylib.WHITE);
            font.draw("REC", vraylib.vec2(float64(app.width()) - 120.0, 60.0), 25.0, vraylib.RED);
            app.end();
    }
    frames = app.frames();
} catch (error) {
    out(error);
    app.close();
    exit(1);
}
app.close();
out("BODYCAM_COMPLETE frames=", frames, " live_handles=", vraylib.live_handles());
