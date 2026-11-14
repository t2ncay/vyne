use external "vraylib/vraylib.vy";
use external "vimage/vimage.vy";
ruleset { dynamic_casting };

fn check(ok :: Bool, message :: String) -> Bool {
    if !ok { throw message; }
    return true;
}

vraylib.set_config_flags(vraylib.FLAG_WINDOW_HIDDEN);
check(vraylib.open(64, 64, "vraylib acceptance", 0), "window initialization");
target := vraylib.load_render_texture(64, 64);
attachment := vraylib.render_texture(target);
baseline := vraylib.live_handles();
through frame :: 0..9 -> loop {
    region temporary {
        vraylib.begin_texture_mode(target);
        vraylib.clear_background(vraylib.BLACK);
        vraylib.draw_rectangle(0, 0, 64, 64, vraylib.RED);
        # Mix raylib with a flushed rlgl batch and restore the texture state.
        vraylib.flush();
        vraylib.gl_set_texture(0);
        vraylib.gl_begin(vraylib.RL_TRIANGLES);
        vraylib.gl_color4ub(0, 255, 0, 255);
        vraylib.gl_vertex2f(0.0, 0.0);
        vraylib.gl_vertex2f(16.0, 0.0);
        vraylib.gl_vertex2f(0.0, 16.0);
        vraylib.gl_end();
        vraylib.flush();
        vraylib.end_texture_mode();
        vraylib.begin_drawing();
        vraylib.present_target(target, 64.0, 64.0);
        vraylib.end_drawing();
    };
};
check(vraylib.live_handles() == baseline, "render attachments must not leak handles per frame");
image := vraylib.load_image_from_texture(attachment);
pixels := vraylib.load_image_colors(image);
pixel := vraylib.buffer_read(pixels, 32 * 64 + 32);
check(pixel.r == 230 && pixel.g == 41 && pixel.b == 55, "GPU pixel readback");
check(vraylib.export_image(image, "build/vraylib-tests/render.png"), "PNG export");
decoded := vimage.load("build/vraylib-tests/render.png");
check(decoded.width == 64 && decoded.height == 64, "vimage/stb coexistence");
vraylib.unload_image_colors(pixels);
vraylib.unload_image(image);

failed :: Bool := false;
try { vraylib.unload_texture(attachment); } catch (error) { failed = true; }
check(failed, "borrowed GPU attachment cannot be unloaded");
vraylib.unload_render_texture(target);
check(!vraylib.valid(attachment), "parent unload invalidates attachment");
mesh := vraylib.gen_mesh_cube(1.0, 1.0, 1.0);
model := vraylib.load_model_from_mesh(mesh);
check(vraylib.is_model_valid(model), "mesh/model ownership transfer");
surface := vraylib.gen_image_color(2, 2, vraylib.BLUE);
texture := vraylib.load_texture_from_image(surface);
vraylib.model_texture(model, texture, 0);
failed = false;
try { vraylib.unload_texture(texture); } catch (error) { failed = true; }
check(failed, "model dependency prevents early texture unload");
failed = false;
try { vraylib.unload_mesh(mesh); } catch (error) { failed = true; }
check(failed, "transferred mesh cannot be unloaded separately");
vraylib.unload_model(model);
check(!vraylib.valid(mesh), "model unload invalidates transferred mesh");
vraylib.unload_texture(texture);
vraylib.unload_image(surface);

vao := vraylib.gl_load_vertex_array();
check(vraylib.valid(vao), "rlgl VAO is an owned handle");
vraylib.gl_unload_vertex_array(vao);
check(!vraylib.valid(vao), "rlgl VAO unload");
cpu_image := vraylib.gen_image_color(4, 4, vraylib.BLUE);
gpu_texture := vraylib.load_texture_from_image(cpu_image);
vraylib.close_window();
check(!vraylib.valid(gpu_texture), "CloseWindow invalidates tracked GPU resources");
check(vraylib.is_image_valid(cpu_image), "CPU images survive graphics context close");
vraylib.shutdown();
check(vraylib.live_handles() == 0, "graphics shutdown");
out("VRAYLIB_RENDER_OK");
