use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.app_options(64, 64, "managed acceptance", 0, vraylib.FLAG_WINDOW_HIDDEN + vraylib.FLAG_WINDOW_RESIZABLE);
app.assets("examples/projects/raylib-showcase/tests");
shader :: vraylib.Shader := app.load_shader("managed.vs", "managed.fs");
alias :: vraylib.Shader := app.load_shader("managed.vs", "managed.fs");
check(alias.token == shader.token, "shader asset cache");
source :: vraylib.RenderTarget := app.target(16, 16);
view :: vraylib.Texture := source.texture();
output :: vraylib.RenderTarget := app.screen_target();
shader.set("tint", vraylib.Vector4(1.0, 1.0, 1.0, 1.0));
shader.set("factor", 1.0);
shader.set("selector", 0);
shader.set("dimensions", vraylib.vec2(16.0, 16.0));
shader.set("shift", vraylib.vec3(0.0, 0.0, 0.0));
shader.set_ints("counts", [0, 0, 0]);
shader.set("transform", vraylib.math_matrix_identity());
shader.set_texture("sourceTex", view);
textured :: vraylib.Model := app.cube(view);
check(app.uniform_queries() == 8, "uniform locations cached once");
app.limit(5);
while app.frame() {
    if app.frames() == 1 { source.resize(32, 32); }
    source.begin(); app.clear(vraylib.RED); source.end();
    output.begin();
    shader.begin();
    source.present(float64(output.width()), float64(output.height()));
    shader.end();
    output.end();
    app.begin(); output.present(float64(app.width()), float64(app.height())); app.end();
    shader.set("factor", 1.0);
}
check(view.valid() && view.width() == 32, "borrowed managed texture follows target replacement");
check(!view.close(), "borrowed texture cannot close its target");
check(app.uniform_queries() == 8, "warm uniforms avoid new GL queries");
pixels_image := vraylib.load_image_from_texture(output.texture().native_handle());
pixels := vraylib.load_image_colors(pixels_image);
pixel := vraylib.buffer_read(pixels, 32 * 64 + 32);
check(pixel.r == 230 && pixel.g == 41 && pixel.b == 55, "resized cached sampler rendered correct pixels");
vraylib.unload_image_colors(pixels); vraylib.unload_image(pixels_image);
app.limit(7);
vraylib.set_window_size(96, 80);
while app.frame() { app.begin(); app.clear(vraylib.BLUE); app.end(); }
check(output.width() == app.width() && output.height() == app.height(), "screen target follows window resize");
failed :: Bool := false;
try { source.close(); } catch (error) { failed = true; }
check(failed, "sampler retains source target");
shader.close();
check(!alias.valid(), "shader aliases invalidate together");
textured.close();
source.close();
app.close();
check(vraylib.live_handles() == 0, "managed graphics cleanup");

draws :: Int64 := 0;
updates :: Int64 := 0;
fn update(owner :: vraylib.App) -> Bool { updates = updates + 1; return true; }
fn draw(owner :: vraylib.App) -> Bool { draws = draws + 1; owner.clear(vraylib.BLUE); return true; }
runner :: vraylib.App := vraylib.app_options(32, 32, "callback acceptance", 0, vraylib.FLAG_WINDOW_HIDDEN);
runner.limit(3);
check(runner.run(update, draw) == 3, "App.run returns completed frames");
check(draws == 3 && updates == 3, "callbacks execute exactly once per frame");
check(!runner.valid() && vraylib.live_handles() == 0, "run owns complete cleanup");

fn throw_in_pass() -> Bool { throw "nested render error"; }
failing :: vraylib.App := vraylib.app_options(32, 32, "pass failure", 0, vraylib.FLAG_WINDOW_HIDDEN);
target :: vraylib.RenderTarget := failing.target(16, 16);
check(failing.frame(), "failure frame starts");
failed = false;
try { target.with_draw(throw_in_pass); } catch (error) {
    check(error == "nested render error", "pass preserves the original exception");
    check(failing.passes() == 0, "finally unwinds target pass");
    failing.close();
    check(error == "nested render error", "exception survives app.close");
    failed = true;
}
check(failed && vraylib.live_handles() == 0, "exceptional pass cleanup");
out("VRAYLIB_MANAGED_RENDER_OK");
