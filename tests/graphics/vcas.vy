ruleset { dynamic_casting };

module vglib;

winW    :: Int64 = 1280;
winH    :: Int64 = 720;
renderW :: Int64 = 640;
renderH :: Int64 = 360;

vglib.init(winW, winH, 60, "Vyne Engine - CAS Spatial Upscaler");

target = vglib.load_render_texture(renderW, renderH);
upscaleShader = vglib.load_shader("upscale.fs");

vglib.begin_shader(upscaleShader);
vglib.set_shader_value(upscaleShader, "textureSize", [float64(renderW), float64(renderH)]);
vglib.end_shader();

cam = vglib.camera(45.0);
vglib.set_pos(cam, 30.0, 0, 0.0);
vglib.disable_cursor();

while (vglib.running()) {
    
    vglib.rotate_view(cam, 0.05);
    if (vglib.key_down(vglib.W)) vglib.move_forward(cam, 0.1);
    if (vglib.key_down(vglib.S)) vglib.move_forward(cam, -0.1);
    if (vglib.key_down(vglib.D)) vglib.move_right(cam, 0.1);
    if (vglib.key_down(vglib.A)) vglib.move_right(cam, -0.1);

    vglib.begin_texture_mode(target);
        vglib.clear(vglib.BLACK);
        
        vglib.begin3d(cam);
            vglib.grid(20, 1.0);
            vglib.cube(0.0, 1.0, 0.0, 2.0, 0.0, vglib.RED);
            vglib.cube(5.0, 1.0, -5.0, 1.5, 45.0, vglib.BLUE);
        vglib.end3d();
        
        vglib.text("Internal Low-Res: 640x360", winW, winH, 20, vglib.GREEN);
    vglib.end_texture_mode();

    vglib.begin();
        vglib.clear(vglib.BLACK);
        
        vglib.begin_shader(upscaleShader);
            vglib.draw_render_texture(target);
        vglib.end_shader();
        
        vglib.text("Upscaled Window: 1280x720 (CAS Filter)", 10, 10, 20, vglib.WHITE);
        vglib.text("FPS: " + string(vglib.get_fps()), 10, 35, 20, vglib.WHITE);
    vglib.end();
}

vglib.close();