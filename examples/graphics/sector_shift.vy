# Sector 4 Incident: original corridor/story, managed vraylib resources and frames.
# WASD movement; Shift sprint; Tab mouse capture; F11 fullscreen; Escape exit.
use external "vraylib/vraylib.vy";
use "sector_shift_state.vy";
use native vmath;
ruleset { dynamic_casting };

fn setting(name :: String, fallback :: Int64) -> Int64 {
    value := vraylib.environment(name);
    if value == null { return fallback; }
    return int64(value);
}
fn asset_root() -> String {
    roots := ["examples/assets", vraylib.get_application_directory() + "../assets", vraylib.get_application_directory() + "../examples/assets"];
    through root :: roots -> loop { if vraylib.file_exists(root + "/wall.jpeg") { return root; } };
    throw "Sector Shift asset directory not found";
}
fn walk(camera :: vraylib.Camera, forward :: Float64, right :: Float64, walls :: Array) -> Bool {
    previous :: vraylib.Vector3 := camera.position();
    camera.move(forward, right);
    candidate :: vraylib.Vector3 := camera.position();
    blocked_x :: Bool := false;
    blocked_z :: Bool := false;
    through wall :: walls -> loop {
        box := vraylib.cube_box(wall[0], wall[1], wall[2], wall[3]);
        if vraylib.check_collision_boxes(vraylib.player_box(vraylib.vec3(candidate.x, previous.y, previous.z), 0.8, 1.8), box) { blocked_x = true; }
        if vraylib.check_collision_boxes(vraylib.player_box(vraylib.vec3(previous.x, previous.y, candidate.z), 0.8, 1.8), box) { blocked_z = true; }
    };
    if blocked_x { candidate.x = previous.x; }
    if blocked_z { candidate.z = previous.z; }
    camera.set_position(candidate);
    return candidate.x != previous.x || candidate.z != previous.z;
}

width :: Int64 := setting("VYNE_SECTOR_WIDTH", 1920);
height :: Int64 := setting("VYNE_SECTOR_HEIGHT", 1080);
frame_limit :: Int64 := setting("VYNE_SECTOR_FRAMES", 0);
autoplay :: Bool := setting("VYNE_SECTOR_AUTOPLAY", 0) != 0;
audio :: Bool := setting("VYNE_SECTOR_AUDIO", 1) != 0;
windowed :: Bool := setting("VYNE_SECTOR_WINDOWED", 0) != 0;
if autoplay && frame_limit == 0 { frame_limit = 60; }
flags :: Int64 := vraylib.FLAG_VSYNC_HINT + vraylib.FLAG_WINDOW_RESIZABLE;
if !windowed && !autoplay { flags = flags + vraylib.FLAG_FULLSCREEN_MODE; }
fps :: Int64 := 60;
if autoplay { fps = 0; flags = vraylib.FLAG_WINDOW_RESIZABLE; }

app :: vraylib.App := vraylib.App(0);
state :: SectorShift.State := SectorShift.initial();
frames :: Int64 := 0;
radio_count :: Int64 := 0;
dsp_attempted :: Bool := false;
dsp_active :: Bool := false;
dsp_blocks :: Int64 := 0;
mouse_captured :: Bool := true;
ambiance :: vraylib.Music := vraylib.Music(0, 0);
radio :: vraylib.Sound := vraylib.Sound(0, 0);

walls :: Array := [];
through i :: 0..25 -> loop {
    z :: Float64 := float64(i) * 20.0;
    # [x, y, z, size]: paired 20-unit walls form the original long corridor.
    walls.push([-15.0, 10.0, z, 20.0]);
    walls.push([15.0, 10.0, z, 20.0]);
};
walls.push([0.0, 50.0, 500.0, 100.0]);

try {
    app = vraylib.app_options(width, height, "Vyne Pro - Sector 4 Incident", fps, flags);
    app.limit(frame_limit);
    app.assets(asset_root());
    camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0, 1.8, -5.0), vraylib.vec3(0.0, 1.8, -4.0), 90.0);
    app.capture_mouse(!autoplay);
    fog :: vraylib.Shader := app.load_shader("../graphics/shaders/fog.vs", "../graphics/shaders/sector_fog.fs");
    vhs :: vraylib.Shader := app.load_shader(null, "../graphics/shaders/sector_vhs.fs");
    scanline :: vraylib.Shader := app.load_shader(null, "../graphics/shaders/scanline.fs");
    building_texture :: vraylib.Texture := app.load_texture("wall.jpeg");
    ground_texture :: vraylib.Texture := app.load_texture("asphalt_road_3.jpg");
    stalker :: vraylib.Texture := app.load_texture("ebil_qehbe.png");
    font :: vraylib.Font := app.load_font("VCR_OSD_MONO_1.001.ttf");
    building :: vraylib.Model := app.cube(building_texture);
    ground :: vraylib.Model := app.plane(ground_texture, 100.0, 700.0);
    building.set_shader(fog);
    ground.set_shader(fog);
    scene :: vraylib.RenderTarget := app.screen_target();
    bodycam :: vraylib.RenderTarget := app.screen_target();
    if audio {
        ambiance = app.music("akira.wav", 0.5);
        radio = app.load_sound("radio_log.wav");
        radio.volume(1.0);
        out("SECTOR_AUDIO active=", ambiance.valid() && radio.valid());
    }

    while app.frame() {
        dt :: Float64 := app.dt();
        frame :: Int64 := app.frames();
        # Deterministic bounded acceptance mode exercises every story threshold.
        # Clock jumps cover the two three-second freezes and final fade quickly.
        if autoplay {
            dt = 1.0 / 60.0;
            z :: Float64 := -5.0;
            if frame >= 5 { z = 55.0; }
            if frame >= 10 { z = 105.0; }
            if frame >= 15 { z = 120.0; }
            if frame >= 21 { z = 155.0; }
            if frame >= 25 { z = 300.0; }
            if frame >= 30 { z = 360.0; }
            if frame == 20 || frame == 26 { dt = 3.1; }
            if frame == 31 { dt = 3.2; }
            camera.set_position(vraylib.vec3(0.0, 1.8, z));
        }
        position :: vraylib.Vector3 := camera.position();
        state = SectorShift.step(state, position.z, dt);
        if state.radio_now && radio.play() { radio_count = radio_count + 1; }
        if state.game_event == 2 && !dsp_attempted {
            dsp_attempted = true;
            if audio { dsp_active = app.saturation(0.8, 1); }
        }
        if state.final_triggered {
            ambiance.volume(0.5 * (1.0 - state.fade));
            radio.volume(1.0 - state.fade);
        }

        if vraylib.is_key_pressed(vraylib.KEY_F11) { vraylib.toggle_fullscreen(); }
        if vraylib.is_key_pressed(vraylib.KEY_TAB) {
            if mouse_captured { mouse_captured = false; app.capture_mouse(false); }
            else { mouse_captured = true; app.capture_mouse(true); }
        }
        is_moving :: Bool := false;
        sprinting :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT);
        if !autoplay && !state.freeze && state.fade <= 0.9 {
            if mouse_captured { camera.look(); }
            forward :: Float64 := 0.0;
            right :: Float64 := 0.0;
            if vraylib.is_key_down(vraylib.KEY_W) { forward = forward + 1.0; }
            if vraylib.is_key_down(vraylib.KEY_S) { forward = forward - 1.0; }
            if vraylib.is_key_down(vraylib.KEY_A) { right = right - 1.0; }
            if vraylib.is_key_down(vraylib.KEY_D) { right = right + 1.0; }
            if forward != 0.0 && right != 0.0 { forward = forward * 0.70710678; right = right * 0.70710678; }
            speed :: Float64 := 7.2;
            if sprinting { speed = speed * 2.5; }
            is_moving = walk(camera, forward * speed * dt, right * speed * dt, walls);
        }
        bob :: Float64 := 0.0;
        if is_moving {
            bob_frequency :: Float64 := 10.0;
            bob_amplitude :: Float64 := 0.05;
            if sprinting { bob_frequency = 16.0; bob_amplitude = 0.12; }
            bob = vmath.sin(state.time * bob_frequency) * bob_amplitude;
        }
        camera.set_height(1.8 + bob);
        fog.set("cameraPos", camera.position());
        fog.set("u_density", state.fog_density);
        scene.begin();
        app.clear(vraylib.rgba(10, 20, 45, 255));
        camera.begin();
        ground.draw(vraylib.vec3(0.0, 0.0, 250.0), 1.0, vraylib.WHITE);
        through wall :: walls -> loop {
            building.draw_scaled(vraylib.vec3(wall[0], wall[1], wall[2]), vraylib.vec3(wall[3], wall[3], wall[3]), vraylib.WHITE);
        };
        # Billboard vertices are already world-space; reset the previous model's
        # transform so the fog distance is correct, and preserve PNG transparency.
        fog.set("matModel", vraylib.math_matrix_identity());
        fog.begin();
        camera.billboard(stalker, vraylib.vec3(2.0, 1.5, state.stalker_z), 3.0, vraylib.WHITE);
        fog.end();
        camera.end();
        scene.end();

        video_time :: Float64 := state.time;
        if state.game_event == 2 { video_time = video_time * 5.0; }
        vhs.set("time", video_time);
        vhs.set("noise_amount", state.glitch);
        vhs.set("renderSize", vraylib.vec2(float64(scene.width()), float64(scene.height())));
        bodycam.begin(); app.clear(vraylib.BLACK); vhs.begin();
        scene.present(float64(bodycam.width()), float64(bodycam.height()));
        vhs.end(); bodycam.end();
        scanline.set("renderSize", vraylib.vec2(float64(app.width()), float64(app.height())));
        app.begin(); app.clear(vraylib.BLACK); scanline.begin();
        bodycam.present(float64(app.width()), float64(app.height()));
        scanline.end();
        font.draw("AXON BODY 3 | UNIT 402", vraylib.vec2(60.0, 60.0), 24.0, vraylib.WHITE);
        font.draw("LAT: 40.4093 N | LON: 49.8671 E", vraylib.vec2(60.0, 90.0), 16.0, vraylib.GRAY);
        font.draw(SectorShift.message(state), vraylib.vec2(60.0, float64(app.height()) - 80.0), 25.0, vraylib.RED);
        if vmath.sin(state.time * 4.0) > 0.0 { font.draw("REC", vraylib.vec2(float64(app.width()) - 170.0, 60.0), 30.0, vraylib.RED); }
        if state.final_triggered {
            vraylib.draw_rectangle(0, 0, app.width(), app.height(), vraylib.rgba(0, 0, 0, int64(state.fade * 255.0)));
            if state.fade >= 0.95 { font.draw("YOU NEVER EXISTED", vraylib.vec2(float64(app.width()) * 0.365, float64(app.height()) * 0.417), 50.0, vraylib.RED); }
        }
        if state.freeze {
            vraylib.draw_rectangle(0, 0, app.width(), app.height(), vraylib.BLACK);
            shake :: Float64 := vmath.sin(state.time * 60.0) * 2.0;
            if vmath.sin(state.time * 25.0) > 0.0 { font.draw("THE WALLS ARE MOVING...", vraylib.vec2(float64(app.width()) * 0.339 + shake, float64(app.height()) * 0.481 + shake), 45.0, vraylib.RED); }
            if vmath.sin(state.time * 100.0) > 0.8 { vraylib.draw_rectangle(0, int64(vmath.abs(vmath.sin(state.time)) * float64(app.height())), app.width(), 2, vraylib.GRAY); }
        }
        app.end();
    }
    frames = app.frames();
    dsp_blocks = app.saturation_blocks();
    out("SECTOR_STATE events=", state.event_count, " phase=", state.game_event, " fade=", state.fade,
        " radio=", radio_count, " dsp=", dsp_active, " dsp_blocks=", dsp_blocks);
} catch (error) {
    out(error); app.close(); exit(1);
}
app.close();
out("SECTOR_COMPLETE frames=", frames, " live_handles=", vraylib.live_handles());
