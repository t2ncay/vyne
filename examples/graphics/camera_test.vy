# Vyne Pro - Bodycam Horror. Original scene preserved, ported to raylib 6.2.
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

fn find_asset(name :: String, shader :: Bool) -> String {
    relative :: String := "examples/assets/";
    local :: String := "../assets/";
    if shader { relative = "examples/graphics/shaders/"; local = "shaders/"; }
    roots := [relative, vraylib.get_application_directory() + local, vraylib.get_application_directory() + "../" + relative];
    through root :: roots -> loop {
        path :: String := root + name;
        if vraylib.file_exists(path) { return path; }
    };
    throw "Bodycam asset not found: " + name;
}

fn require_resource(ok :: Bool, name :: String) -> Bool {
    if !ok { throw "Bodycam resource could not be loaded: " + name; }
    return true;
}

# Axis-separated horizontal collision preserves sliding along building walls.
fn walk(camera :: Int64, forward :: Float64, right :: Float64, walls :: Array, height :: Float64) -> Bool {
    previous :: vraylib.Vector3 := vraylib.camera_position(camera);
    vraylib.camera_step(camera, forward, right);
    candidate :: vraylib.Vector3 := vraylib.camera_position(camera);
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
    vraylib.camera_set_position(camera, candidate);
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

# The shutdown registry also handles partially completed initialization.
try {
    vraylib.set_config_flags(vraylib.FLAG_VSYNC_HINT);
    require_resource(vraylib.open(width, height, "Vyne Pro - Bodycam Horror", 75), "window");
    camera :: Int64 := vraylib.camera_create(vraylib.vec3(30.0, 1.8, 0.0), vraylib.vec3(30.0, 1.8, 1.0), 80.0);
    vraylib.disable_cursor();

    fog := vraylib.load_shader(find_asset("fog.vs", true), find_asset("fog.fs", true));
    vhs := vraylib.load_shader(null, find_asset("vhs.fs", true));
    scanline := vraylib.load_shader(null, find_asset("scanline.fs", true));
    require_resource(vraylib.is_shader_valid(fog) && vraylib.is_shader_valid(vhs) && vraylib.is_shader_valid(scanline), "post-processing shaders");
    default_shader :: Int64 := vraylib.gl_get_shader_id_default();
    require_resource(vraylib.field(fog, "id", 0) != default_shader && vraylib.field(vhs, "id", 0) != default_shader && vraylib.field(scanline, "id", 0) != default_shader, "custom shader compilation (default fallback is not sufficient)");
    font := vraylib.load_font(find_asset("VCR_OSD_MONO_1.001.ttf", false));
    require_resource(vraylib.is_font_valid(font), "VCR OSD font");
    building_texture := vraylib.load_texture(find_asset("building.jpg", false));
    ground_texture := vraylib.load_texture(find_asset("asphalt_road_3.jpg", false));
    require_resource(vraylib.is_texture_valid(building_texture) && vraylib.is_texture_valid(ground_texture), "building/ground textures");
    building := vraylib.textured_cube(building_texture);
    ground := vraylib.textured_plane(ground_texture, 200.0, 200.0);
    cat := vraylib.load_model(find_asset("12222_Cat_v1_l3.obj", false));
    cat_texture := vraylib.load_texture(find_asset("Cat_diffuse.jpg", false));
    require_resource(vraylib.is_model_valid(cat) && vraylib.is_texture_valid(cat_texture), "cat model/texture");
    vraylib.model_texture(cat, cat_texture, 0);
    airplane := vraylib.load_model(find_asset("11803_Airplane_v1_l1.obj", false));
    require_resource(vraylib.is_model_valid(airplane), "airplane model");
    airplane_textures := [
        vraylib.load_texture(find_asset("11803_Airplane_body_diff.jpg", false)),
        vraylib.load_texture(find_asset("11803_Airplane_tail_diff.jpg", false)),
        vraylib.load_texture(find_asset("11803_Airplane_wing_big_L_diff.jpg", false)),
        vraylib.load_texture(find_asset("11803_Airplane_wing_big_R_diff.jpg", false))
    ];
    material_count :: Int64 := vraylib.field(airplane, "materialCount", 0);
    through i :: 0..3 -> loop {
        require_resource(vraylib.is_texture_valid(airplane_textures[i]), "airplane texture");
        if i < material_count { vraylib.model_texture(airplane, airplane_textures[i], i); }
    };
    vraylib.model_shader(building, fog);
    vraylib.model_shader(ground, fog);
    vraylib.model_shader(cat, fog);
    vraylib.model_shader(airplane, fog);

    scene := vraylib.load_render_texture(width, height);
    bodycam := vraylib.load_render_texture(width, height);
    require_resource(vraylib.is_render_texture_valid(scene) && vraylib.is_render_texture_valid(bodycam), "render targets");
    uniform_storage := vraylib.buffer("float", 3);
    fog_camera := vraylib.get_shader_location(fog, "cameraPos");
    vhs_time := vraylib.get_shader_location(vhs, "time");
    vhs_size := vraylib.get_shader_location(vhs, "renderSize");
    scanline_size := vraylib.get_shader_location(scanline, "renderSize");
    music :: Int64 := 0;
    if setting("VYNE_CAMERA_AUDIO", 1) != 0 {
        music = vraylib.ambiance(find_asset("akira.wav", false), 1.0);
        out("BODYCAM_AUDIO active=", music != 0);
    }

    while !vraylib.window_should_close() && (frame_limit == 0 || frames < frame_limit) {
        region frame {
            dt :: Float64 := vmath.min(vraylib.get_frame_time(), 0.05);
            elapsed = elapsed + dt;
            if vraylib.is_key_pressed(vraylib.KEY_F11) { vraylib.toggle_fullscreen(); }
            if vraylib.is_key_pressed(vraylib.KEY_TAB) {
                if mouse_captured {
                    mouse_captured = false;
                    vraylib.enable_cursor();
                } else {
                    mouse_captured = true;
                    vraylib.disable_cursor();
                }
            }
            if mouse_captured { vraylib.camera_mouse_look(camera, 0.15); }
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

            position :: vraylib.Vector3 := vraylib.camera_position(camera);
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
            vraylib.camera_set_height(camera, eye_y);
            position = vraylib.camera_position(camera);
            if music != 0 { vraylib.update_music_stream(music); }
            cat_x = cat_x - elapsed * 0.6 * dt;
            air_x = air_x + 48.0 * dt;
            air_y = air_y - 4.8 * dt;

            vraylib.uniform_vec3(fog, fog_camera, uniform_storage, position.x, position.y, position.z);
            vraylib.begin_texture_mode(scene);
            vraylib.clear_background(vraylib.rgba(128, 128, 140, 255));
            vraylib.begin_mode_3d(vraylib.camera_view(camera));
            vraylib.model_at(ground, vraylib.vec3(0.0, 0.0, 0.0), vraylib.vec3(1.0, 1.0, 1.0), vraylib.WHITE);
            through wall :: walls -> loop {
                vraylib.model_at(building, vraylib.vec3(wall[0], wall[1], wall[2]), vraylib.vec3(wall[3], wall[3], wall[3]), vraylib.WHITE);
            };
            vraylib.draw_model(cat, vraylib.vec3(cat_x, 2.5 + vmath.sin(elapsed * 2.0) * 0.2, 25.0), 0.05, vraylib.WHITE);
            vraylib.draw_model(airplane, vraylib.vec3(air_x, air_y, -50.0), 0.02, vraylib.WHITE);
            vraylib.end_mode_3d();
            vraylib.end_texture_mode();

            vraylib.uniform_float(vhs, vhs_time, uniform_storage, elapsed);
            vraylib.uniform_vec2(vhs, vhs_size, uniform_storage, float64(width), float64(height));
            vraylib.begin_texture_mode(bodycam);
            vraylib.clear_background(vraylib.BLACK);
            vraylib.begin_shader_mode(vhs);
            vraylib.present_target(scene, float64(width), float64(height));
            vraylib.end_shader_mode();
            vraylib.end_texture_mode();

            vraylib.uniform_vec2(scanline, scanline_size, uniform_storage, float64(width), float64(height));
            vraylib.begin_drawing();
            vraylib.clear_background(vraylib.BLACK);
            vraylib.begin_shader_mode(scanline);
            vraylib.present_target(bodycam, float64(vraylib.get_screen_width()), float64(vraylib.get_screen_height()));
            vraylib.end_shader_mode();
            vraylib.draw_text_ex(font, "AXON BODY 3 - UNIT 402", vraylib.vec2(60.0, 60.0), 20.0, 1.0, vraylib.WHITE);
            vraylib.draw_text_ex(font, "2026-04-21 01:14:23", vraylib.vec2(60.0, 90.0), 18.0, 1.0, vraylib.WHITE);
            vraylib.draw_text_ex(font, "REC", vraylib.vec2(float64(vraylib.get_screen_width()) - 120.0, 60.0), 25.0, 1.0, vraylib.RED);
            vraylib.end_drawing();
            frames = frames + 1;
        };
    }
    vraylib.enable_cursor();
} catch (error) {
    out(error);
    vraylib.shutdown();
    exit(1);
}
vraylib.shutdown();
out("BODYCAM_COMPLETE frames=", frames, " live_handles=", vraylib.live_handles());
