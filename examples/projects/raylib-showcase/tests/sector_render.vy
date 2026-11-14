use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
fn sample(target :: vraylib.RenderTarget) -> vraylib.Color {
    image := vraylib.load_image_from_texture(target.texture().native_handle());
    pixels := vraylib.load_image_colors(image);
    pixel := vraylib.buffer_read(pixels, 16 * 32 + 16);
    vraylib.unload_image_colors(pixels); vraylib.unload_image(image);
    return pixel;
}
app :: vraylib.App := vraylib.app_options(32, 32, "Sector shader acceptance", 0, vraylib.FLAG_WINDOW_HIDDEN);
app.assets("examples/graphics/shaders");
fog :: vraylib.Shader := app.load_shader("fog.vs", "sector_fog.fs");
vhs :: vraylib.Shader := app.load_shader(null, "sector_vhs.fs");
image :: vraylib.Image := app.image(2, 2, vraylib.RED);
handle := vraylib.load_texture_from_image(image.token);
vraylib.managed_adopt(app.token, handle);
texture :: vraylib.Texture := vraylib.Texture(handle, app.token, true);
model :: vraylib.Model := app.cube(texture);
model.set_shader(fog);
camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0, 0.0, 0.0), vraylib.vec3(0.0, 0.0, -1.0), 60.0);
source :: vraylib.RenderTarget := app.target(32, 32);
output :: vraylib.RenderTarget := app.target(32, 32);
check(app.frame(), "frame starts");
fog.set("cameraPos", camera.position());
fog.set("u_density", 0.0);
source.begin(); app.clear(vraylib.BLACK); camera.begin();
model.draw(vraylib.vec3(0.0, 0.0, -3.0), 2.0, vraylib.WHITE);
camera.end(); source.end();
clear_pixel :: vraylib.Color := sample(source);
fog.set("u_density", 1.0);
source.begin(); app.clear(vraylib.BLACK); camera.begin();
model.draw(vraylib.vec3(0.0, 0.0, -3.0), 2.0, vraylib.WHITE);
camera.end(); source.end();
fog_pixel :: vraylib.Color := sample(source);
check(clear_pixel.r > fog_pixel.r + 40, "fog density changes actual pixels");

vhs.set("time", 1.0);
vhs.set("renderSize", vraylib.vec2(32.0, 32.0));
vhs.set("noise_amount", 0.0);
output.begin(); vhs.begin(); source.present(32.0, 32.0); vhs.end(); output.end();
quiet :: vraylib.Color := sample(output);
vhs.set("noise_amount", 0.8);
output.begin(); vhs.begin(); source.present(32.0, 32.0); vhs.end(); output.end();
noisy :: vraylib.Color := sample(output);
check(quiet.r != noisy.r || quiet.g != noisy.g || quiet.b != noisy.b, "glitch uniform changes actual pixels");
app.end_frame(); app.close();
check(vraylib.live_handles() == 0, "sector shader cleanup");
out("VRAYLIB_SECTOR_RENDER_OK");
