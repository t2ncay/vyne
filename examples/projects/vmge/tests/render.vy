use "../src/engine/render.vy";

fn vmge_check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
fn vmge_pixel(target :: vraylib.RenderTarget, x :: Int64, y :: Int64) -> vraylib.Color {
    image :: Int64 := vraylib.load_image_from_texture(target.texture().native_handle());
    colors :: Int64 := vraylib.load_image_colors(image);
    # GPU render-target readback is bottom-up; expose logical top-left pixels.
    color :: vraylib.Color := vraylib.buffer_read(colors, (target.height() - 1 - y) * target.width() + x);
    vraylib.unload_image_colors(colors);
    vraylib.unload_image(image);
    return color;
}

app :: vraylib.App := vraylib.app_options(64, 64, "VMGE render acceptance", 0, vraylib.FLAG_WINDOW_HIDDEN + vraylib.FLAG_WINDOW_RESIZABLE);
try {
    app.assets(vmge_project_root());
    image :: vraylib.Image := app.image(4, 4, vraylib.RED);
    vmge_check(image.save("build/vmge-tests/partial_assets/Brick/Brick_16-512x512.png"), "create partial asset fixture");
    texture :: vraylib.Texture := app.texture(image);
    image.close();
    vmge_check(texture.valid(), "GPU texture is independent of closed source image");
    failed :: Bool := false;
    try { app.texture(image); } catch (error) { failed = true; }
    vmge_check(failed, "texture factory rejects closed images");
    failed = false;
    try { texture.draw_region(vraylib.Rectangle(0.0, 0.0, 4.0, 4.0), vraylib.Rectangle(0.0, 0.0, 4.0, 4.0), vraylib.vec2(0.0, 0.0), 0.0, vraylib.WHITE); } catch (error) { failed = true; }
    vmge_check(failed && !app.drawing(), "scaled texture draw diagnoses missing frame/surface");
    camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0, 1.8, 0.0), vraylib.vec3(0.0, 1.8, 1.0), 80.0);
    view :: vraylib.Camera3D := camera.view();
    ray :: vraylib.Ray := camera.ray(vraylib.vec2(32.0, 32.0));
    vmge_check(view.position.y > 1.79 && ray.direction.z > 0.99 && vmath.abs(ray.direction.x) < 0.001, "checked camera view and center ray");
    cursor :: vraylib.Vector3 := vmge_grid_cursor(camera, 64, 64, 8.0, 2.0, 1.0);
    vmge_check(cursor.x == 0.0 && cursor.z == 8.0 && cursor.y == 1.0, "center ray snaps to half-height cube grid");
    cursor = vmge_grid_cursor(camera, 64, 64, 8.0, 2.0, 0.0);
    vmge_check(cursor.y == 2.0, "legacy integer-height grid remains selectable");
    source :: vraylib.RenderTarget := app.target(16, 16);
    borrowed :: vraylib.Texture := source.texture();
    output :: vraylib.RenderTarget := app.screen_target();
    scene :: vraylib.RenderTarget := app.screen_target();
    bodycam :: vraylib.RenderTarget := app.screen_target();
    final :: vraylib.RenderTarget := app.screen_target();
    fog :: vraylib.Shader := app.load_shader("shaders/fog.vs", "shaders/fog.fs");
    vhs :: vraylib.Shader := app.load_shader(null, "shaders/vhs.fs");
    scanline :: vraylib.Shader := app.load_shader(null, "shaders/scanline.fs");
    fog_alias :: vraylib.Shader := app.load_shader("shaders/fog.vs", "shaders/fog.fs");
    vmge_check(fog_alias.token == fog.token, "shader cache shares managed tokens");
    textures :: Array := vmge_palette(app, "");
    models :: Array := vmge_models(app, textures);
    through model :: models -> loop { model.set_shader(fog); };
    world :: VMGEMap := vmge_map_parse("0,1,6,2,0;", 8);
    app.limit(4);
    while app.frame() {
        source.begin(); app.clear(vraylib.BLUE); source.end();
        output.begin();
        vmge_check(app.drawing(), "active render target is a drawing surface");
        app.clear(vraylib.BLACK);
        texture.draw_region(vraylib.Rectangle(0.0, 0.0, 4.0, 4.0), vraylib.Rectangle(0.0, 0.0, 64.0, 64.0), vraylib.vec2(0.0, 0.0), 0.0, vraylib.WHITE);
        output.end();
        color :: vraylib.Color := vmge_pixel(output, 32, 32);
        vmge_check(color.r == 230 && color.g == 41 && color.b == 55, "scaled managed texture renders expected pixels");
        if app.frames() == 1 { source.resize(32, 32); }
        source.begin();
        app.clear(vraylib.BLUE);
        vraylib.draw_rectangle(0, 0, source.width(), source.height() / 2, vraylib.RED);
        source.end();
        output.begin();
        borrowed.draw_region(vraylib.Rectangle(0.0, 0.0, float64(borrowed.width()), float64(borrowed.height())), vraylib.Rectangle(0.0, 0.0, 64.0, 64.0), vraylib.vec2(0.0, 0.0), 0.0, vraylib.WHITE);
        output.end();
        color = vmge_pixel(output, 32, 48);
        vmge_check(color.r == 0 && color.g == 121 && color.b == 241, "borrowed texture drawing follows target replacement");
        color = vmge_pixel(output, 32, 8);
        vmge_check(color.r == 230 && color.g == 41 && color.b == 55, "borrowed texture region preserves top-left orientation");
        output.begin();
        borrowed.draw_region(vraylib.Rectangle(0.0, 0.0, float64(borrowed.width()), float64(borrowed.height()) * 0.5), vraylib.Rectangle(0.0, 0.0, 64.0, 64.0), vraylib.vec2(0.0, 0.0), 0.0, vraylib.WHITE);
        output.end();
        color = vmge_pixel(output, 32, 48);
        vmge_check(color.r == 230 && color.g == 41 && color.b == 55, "borrowed top-half crop uses logical source coordinates");
        fog.set("cameraPos", camera.position());
        scene.begin(); app.clear(vraylib.LIGHTGRAY); camera.begin();
        vmge_draw_map(world, models, camera.position(), 100.0);
        camera.end(); scene.end();
        vhs.set("time", 0.5);
        bodycam.begin(); app.clear(vraylib.BLACK); vhs.begin();
        scene.present(float64(bodycam.width()), float64(bodycam.height()));
        vhs.end(); bodycam.end();
        scanline.set("renderSize", vraylib.vec2(float64(final.width()), float64(final.height())));
        final.begin(); app.clear(vraylib.BLACK); scanline.begin();
        bodycam.present(float64(final.width()), float64(final.height()));
        scanline.end(); final.end();
        original :: vraylib.Color := vmge_pixel(scene, 32, 32);
        processed :: vraylib.Color := vmge_pixel(final, 32, 32);
        vmge_check(original.r > 0 && processed.r > 0 && (original.r != processed.r || original.g != processed.g || original.b != processed.b), "fog scene and bodycam postprocessing render nonblack changed pixels");
        app.begin(); final.present(float64(app.width()), float64(app.height())); app.end();
        vmge_check(app.passes() == 0 && app.uniform_queries() == 3, "passes balance and uniform locations stay cached");
    }
    vraylib.set_window_size(96, 80);
    app.limit(6);
    while app.frame() { app.begin(); app.clear(vraylib.BLACK); app.end(); }
    vmge_check(scene.width() == app.width() && bodycam.height() == app.height() && final.width() == app.width() && app.width() != 64, "all postprocess targets follow actual window resize");
    texture.close();
    failed = false;
    try { texture.draw_region(vraylib.Rectangle(0.0, 0.0, 4.0, 4.0), vraylib.Rectangle(0.0, 0.0, 4.0, 4.0), vraylib.vec2(0.0, 0.0), 0.0, vraylib.WHITE); }
    catch (error) { failed = true; }
    vmge_check(failed, "scaled drawing rejects closed textures");
    camera.close();
    failed = false;
    try { camera.ray(vraylib.vec2(0.0, 0.0)); } catch (error) { failed = true; }
    vmge_check(failed, "picking rejects closed cameras");
} finally { app.close(); }
vmge_check(vraylib.live_handles() == 0, "render and procedural resources released");

# A frame failure must unwind surface/camera passes and preserve the exception.
app = vraylib.app_options(32, 32, "VMGE failure acceptance", 0, vraylib.FLAG_WINDOW_HIDDEN);
failed :: Bool := false;
try {
    camera :: vraylib.Camera := app.first_person();
    app.frame(); app.begin(); camera.begin();
    throw "VMGE nested rendering failure";
} catch (error) {
    app.close();
    vmge_check(error == "VMGE nested rendering failure", "frame exception survives managed cleanup");
    failed = true;
} finally { app.close(); }
vmge_check(failed && vraylib.live_handles() == 0, "exception unwinds active passes and releases session");
out("VMGE_RENDER_OK");
