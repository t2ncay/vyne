use "../src/engine/world_render.vy";
use native vmem;
fn world_render_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.app_options(64,64,"world graphics acceptance",0,vraylib.FLAG_WINDOW_HIDDEN);
@bounded<33554432> region world_render_owner {
    world :: SEWorld := se_world_create(); se_terrain_generate(world,64,1.0,0.0,42,3);
    se_world_rectangle(world,0,0,0,15,15,1.0,1.5,2,false);
    camera :: vraylib.Camera := app.camera(vraylib.vec3(32.0,80.0,-30.0),vraylib.vec3(32.0,0.0,32.0),60.0);
    target :: vraylib.RenderTarget := app.target(64,64);
    warm :: Int64 := vmem.total_allocated(); committed :: Int64 := vmem.commit_total_allocated(); app.limit(32);
    try {
        while app.frame() {
            target.begin(); app.clear(vraylib.BLACK); camera.begin();
            try { se_world_draw(world); } finally { camera.end(); } target.end();
            ray :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(32.0,32.0),64,64);
            world_render_check(se_world_ray(world,ray) && vmath.abs(world.work[4227]) < 0.00001 && world.work[4232] == -1.0,"rendered terrain and pick surface agree");
            if app.frames() == 1 {
                image :: Int64 := vraylib.load_image_from_texture(target.texture().native_handle()); colors :: Int64 := vraylib.load_image_colors(image);
                pixel :: vraylib.Color := vraylib.buffer_read(colors,31*64+32);
                vraylib.unload_image_colors(colors); vraylib.unload_image(image);
                world_render_check(pixel.g > pixel.r && pixel.g > pixel.b,"bounded grid renders visible green terrain");
            }
            app.begin(); target.present(64.0,64.0); app.end();
            world_render_check(app.passes() == 0 && world.data.size() == 24736 && world.work.size() == 4256 && world.points.size() == 4 && world.colors.size() == 10,"fixed resource storage and balanced passes");
        }
        world_render_check(vmem.total_allocated()-warm <= 512 && vmem.commit_total_allocated() == committed,"max terrain drawing does not retain per-triangle boxes");
        out("WORLD_RENDER_MEMORY headers=",vmem.total_allocated()-warm," commit_drift=",vmem.commit_total_allocated()-committed);
    } finally { app.close(); }
};
world_render_check(vraylib.live_handles() == 0,"world graphics handles closed");
out("VMGE_WORLD_RENDER_OK");
