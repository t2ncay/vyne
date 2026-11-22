use "assets.vy";

# Read W/S keys as signed forward movement input.
#
# Returns:
#     The computed Float64 value: value.
fn vmge_forward() -> Float64 {
    value :: Float64 := 0.0;
    if vraylib.is_key_down(vraylib.KEY_W) {
        value = value + 1.0;
    }
    if vraylib.is_key_down(vraylib.KEY_S) {
        value = value - 1.0;
    }
    return value;
}

# Read D/A keys as signed lateral movement input.
#
# Returns:
#     The computed Float64 value: value.
fn vmge_right() -> Float64 {
    value :: Float64 := 0.0;
    if vraylib.is_key_down(vraylib.KEY_D) {
        value = value + 1.0;
    }
    if vraylib.is_key_down(vraylib.KEY_A) {
        value = value - 1.0;
    }
    return value;
}

# Create the managed VMGE window and apply configured size, fullscreen, hidden-window, and frame-limit
# options.
#
# Parameters:
#     title (String): Display title or node name.
#
# Returns:
#     The computed vraylib.App value: app.
fn vmge_window(title :: String) -> vraylib.App {
    width :: Int64 := vmge_setting("VYNE_VMGE_WIDTH", 1280);
    height :: Int64 := vmge_setting("VYNE_VMGE_HEIGHT", 720);
    flags :: Int64 := vraylib.FLAG_WINDOW_RESIZABLE + vraylib.FLAG_VSYNC_HINT;
    if vmge_setting("VYNE_VMGE_WINDOWED", 1) == 0 {
        flags = flags + vraylib.FLAG_FULLSCREEN_MODE;
    }
    if vmge_setting("VYNE_VMGE_HIDDEN", 0) != 0 {
        flags = flags + vraylib.FLAG_WINDOW_HIDDEN;
    }
    app :: vraylib.App := vraylib.app_options(width, height, title, 60, flags);
    app.limit(vmge_setting("VYNE_VMGE_FRAMES", 0));
    return app;
}

# Handle shared cursor capture and fullscreen toggles, returning the updated capture flag.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     captured (Bool): Current cursor-capture flag to update from shared input.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn vmge_capture(app :: vraylib.App, captured :: Bool) -> Bool {
    if vraylib.is_key_pressed(vraylib.KEY_F11) {
        vraylib.toggle_fullscreen();
    }
    if vraylib.is_key_pressed(vraylib.KEY_TAB) {
        app.capture_mouse(!captured);
        return !captured;
    }
    return captured;
}
