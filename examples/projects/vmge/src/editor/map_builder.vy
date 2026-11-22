use "../engine/render.vy";
use "../engine/export.vy";
use "../engine/input.vy";
use "../ui/theme.vy";

fn vmge_editor() -> Int64 {
    app :: vraylib.App := vraylib.App(0);
    frames :: Int64 := 0;
    edits :: Int64 := 0;
    try {
        project :: String := vmge_project_root();
        map_path :: String := vmge_option("VYNE_VMGE_MAP", "tau_map.dat");
        export_path :: String := vmge_option("VYNE_VMGE_EXPORT", "exported_map.obj");
        world :: VMGEMap := vmge_map_load(map_path, vmge_setting("VYNE_VMGE_CAPACITY", 16384), true);
        app = vmge_window("VMGE - Map Builder");
        se_theme_load(app,project);
        textures :: Array := vmge_palette(app, vmge_asset_root(project));
        models :: Array := vmge_models(app, textures);
        camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0, 9.0, -15.0), vraylib.vec3(0.0, 1.0, 0.0), 70.0);
        captured :: Bool := vmge_setting("VYNE_VMGE_HIDDEN", 0) == 0;
        app.capture_mouse(captured);
        slot :: Int64 := 0;
        grid :: Float64 := 2.0;
        origin_y :: Float64 := grid * 0.5;
        if world.count() > 0 { origin_y = world.value(0, 1) - vmath.floor(world.value(0, 1) / grid) * grid; }
        status :: Int64 := 0;
        automated :: Bool := vmge_setting("VYNE_VMGE_AUTOTEST", 0) != 0;
        while app.frame() {
            captured = vmge_capture(app, captured);
            if captured { camera.look(); }
            forward :: Float64 := vmge_forward();
            right :: Float64 := vmge_right();
            if forward != 0.0 && right != 0.0 { forward = forward * 0.70710678; right = right * 0.70710678; }
            camera.move(forward * 24.0 * app.dt(), right * 24.0 * app.dt());
            position :: vraylib.Vector3 := camera.position();
            vertical :: Float64 := 0.0;
            if vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT) { vertical = vertical + 1.0; }
            if vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL) { vertical = vertical - 1.0; }
            camera.set_height(position.y + vertical * 24.0 * app.dt());
            i :: Int64 := 0;
            while i < 8 { if vraylib.is_key_pressed(vraylib.KEY_ONE + i) { slot = i; } i = i + 1; }
            cursor :: vraylib.Vector3 := vmge_grid_cursor(camera, app.width(), app.height(), 8.0, grid, origin_y);
            place :: Bool := vraylib.is_key_pressed(vraylib.KEY_SPACE) || (captured && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT));
            remove :: Bool := vraylib.is_key_pressed(vraylib.KEY_BACKSPACE) || (captured && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_RIGHT));
            save :: Bool := vraylib.is_key_pressed(vraylib.KEY_J);
            export :: Bool := vraylib.is_key_pressed(vraylib.KEY_K);
            # Deterministic acceptance uses the same editing and I/O paths.
            if automated {
                tick :: Int64 := app.frames() + 1;
                cursor = vraylib.vec3(100.0, 1.0, 100.0);
                place = tick == 1 || tick == 2 || tick == 4;
                remove = tick == 3;
                save = tick == 5;
                export = tick == 6;
            }
            if place {
                if world.occupied(cursor.x, cursor.y, cursor.z) { status = 2; }
                else {
                    if world.count() >= world.capacity { status = 7; }
                    else { world.place(cursor.x, cursor.y, cursor.z, grid, slot); status = 1; edits = edits + 1; }
                }
            }
            if remove { if world.remove(cursor.x, cursor.y, cursor.z) > 0 { edits = edits + 1; } status = 3; }
            if save {
                try { vmge_map_save(world, map_path); status = 4; }
                catch (error) { out(error); status = 6; if automated { throw error; } }
            }
            if export {
                try { vmge_map_export(world, export_path); status = 5; }
                catch (error) { out(error); status = 6; if automated { throw error; } }
            }
            pulse :: Int64 := int64(100.0 + 70.0 * vmath.sin(app.elapsed() * 6.0));
            preview :: vraylib.Model := models[slot];
            app.begin();
            app.clear(vraylib.rgba(15, 15, 20, 255));
            camera.begin();
            vraylib.draw_grid(100, grid);
            vmge_draw_map(world, models, camera.position(), 0.0);
            preview.draw(cursor, grid, vraylib.rgba(100, 255, 100, pulse));
            vraylib.draw_cube_wires(cursor, grid, grid, grid, vraylib.GREEN);
            camera.end();
            x :: Int64 := app.width() - 190;
            y :: Int64 := app.height() - 210;
            vraylib.draw_rectangle(x - 8, y - 8, 176, 176, vraylib.rgba(40, 40, 50, 220));
            texture :: vraylib.Texture := textures[slot];
            texture.draw_region(vraylib.Rectangle(0.0, 0.0, float64(texture.width()), float64(texture.height())), vraylib.Rectangle(float64(x), float64(y), 160.0, 160.0), vraylib.vec2(0.0, 0.0), 0.0, vraylib.WHITE);
            se_draw_text("SELECTED SLOT " + string(slot + 1), x, y - 28, 18, vraylib.SKYBLUE);
            se_draw_heading("VMGE MAP BUILDER | " + map_path, 24, 24, 22, vraylib.SKYBLUE);
            se_draw_text("CUBES " + string(world.count()) + "/" + string(world.capacity) + " | SLOT " + string(slot + 1), 24, 54, 18, vraylib.WHITE);
            se_draw_text("WASD move | Shift/Ctrl up/down | 1-8 texture", 24, 84, 16, vraylib.GRAY);
            se_draw_text("Space/left click place | Backspace/right click remove", 24, 108, 16, vraylib.GRAY);
            se_draw_text("J save | K export | Tab mouse | F11 fullscreen | Esc exit", 24, 132, 16, vraylib.GRAY);
            se_draw_text("+", app.width() / 2 - 5, app.height() / 2 - 10, 20, vraylib.WHITE);
            message :: String := "EDITOR READY";
            if status == 1 { message = "OBJECT PLACED"; }
            if status == 2 { message = "CELL ALREADY OCCUPIED"; }
            if status == 3 { message = "OBJECT REMOVED"; }
            if status == 4 { message = "MAP SAVED"; }
            if status == 5 { message = "OBJ GEOMETRY EXPORTED"; }
            if status == 6 { message = "WRITE FAILED - SEE CONSOLE"; }
            if status == 7 { message = "MAP CAPACITY REACHED - REMOVE CUBES OR RESTART WITH A LARGER CAPACITY"; }
            se_draw_text(message, 24, app.height() - 42, 20, vraylib.GREEN);
            app.end();
        }
        frames = app.frames();
        out("VMGE_EDITOR_STATE cubes=", world.count(), " edits=", edits);
    } finally { app.close(); }
    out("VMGE_EDITOR_COMPLETE frames=", frames, " live_handles=", vraylib.live_handles());
    return frames;
}
