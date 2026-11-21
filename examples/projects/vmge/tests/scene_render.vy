use "../src/editor/viewport.vy";
use "../src/scene/history.vy";
use "../src/scene/play.vy";

fn se_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
fn se_sample(target :: vraylib.RenderTarget,x :: Int64,y :: Int64) -> vraylib.Color {
    image :: Int64 := vraylib.load_image_from_texture(target.texture().native_handle());
    colors :: Int64 := vraylib.load_image_colors(image);
    color :: vraylib.Color := vraylib.buffer_read(colors,(target.height()-1-y)*target.width()+x);
    vraylib.unload_image_colors(colors); vraylib.unload_image(image); return color;
}
app :: vraylib.App := vraylib.app_options(64,64,"scene graphics acceptance",0,vraylib.FLAG_WINDOW_HIDDEN);
@slab<Int64,256,512> region scene_storage {
    names :: Array := []; paths :: Array := []; asset_paths :: Array := [];
    i :: Int64 := 0;
    while i < 128 {
        name_bytes :: Array<Int64> := slab_alloc(64); names.push(name_bytes);
        path_bytes :: Array<Int64> := slab_alloc(256); paths.push(path_bytes); i = i+1;
    }
    i = 0; while i < 64 { path_bytes :: Array<Int64> := slab_alloc(256); asset_paths.push(path_bytes); i = i+1; }
    text_bytes :: Array<Int64> := slab_alloc(256);
    scene :: SEScene := se_scene(names,paths,0);
    @pool<Float64,2048> region matrices {
        data :: Array<Float64> := pool_alloc();
        cache :: SETransforms := se_transforms(data);
        try {
            root :: Int64 := scene.spawn(0,"Root",-1);
            child :: Int64 := scene.spawn(1,"Mesh",root);
            scene.property(root,0,3.0); scene.property(child,2,2.0);
            se_update_transforms(scene,cache);
            position :: vraylib.Vector3 := se_position(cache,child%128);
            se_check(vmath.abs(position.x-3.0)<0.001 && vmath.abs(position.z-2.0)<0.001,"hierarchical translation");
            scene.property(root,4,90.0); se_update_transforms(scene,cache);
            position = se_position(cache,child%128);
            se_check(vmath.abs(position.x-5.0)<0.001 && vmath.abs(position.z)<0.001,"hierarchical rotation");
            scene.meta[(root%128)*8+4] = 0; se_check(!se_visible(scene,child),"parent visibility propagates"); scene.meta[(root%128)*8+4] = 1;
            image :: vraylib.Image := app.image(1,1,vraylib.WHITE);
            texture :: vraylib.Texture := app.texture(image); image.close();
            model :: vraylib.Model := app.cube(texture);
            camera :: vraylib.Camera := app.camera(vraylib.vec3(5.0,0.0,-6.0),vraylib.vec3(5.0,0.0,0.0),70.0);
            target :: vraylib.RenderTarget := app.target(64,64);
            matrix :: vraylib.Matrix := se_world(scene,cache,child);
            ray :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(32.0,32.0),64,64);
            hit :: vraylib.RayCollision := model.pick(ray,matrix);
            se_check(hit.hit && vmath.abs(hit.distance-5.5)<0.001,"model picking uses hierarchical matrix");
            bank :: SEAssets := se_assets(app,asset_paths,vmge_project_root());
            cubes :: Array := [model];
            se_check(se_pick(scene,cache,bank,cubes,ray) == child,"viewport node selection");
            se_check(se_asset_relative(bank,bank.root+"/assets/cube.obj") == "assets/cube.obj","project-relative assets");
            ui :: SEUI := se_ui(text_bytes); ui.state[0] = child; ui.state[14] = child;
            ui.state[2] = 0; ui.numbers[12] = 100.0; ui.numbers[13] = 0.0; ui.numbers[14] = 1.0; ui.numbers[11] = 0.0;
            se_gizmo_move(ui,scene,vraylib.vec2(50.0,0.0)); se_check(scene.number(child,0) == 1.0,"translation gizmo snapping");
            ui.state[3] = 1; ui.numbers[11] = 0.0;
            se_gizmo_move(ui,scene,vraylib.vec2(50.0,0.0)); se_check(scene.number(child,3) == 45.0,"rotation gizmo snapping");
            ui.state[3] = 2; ui.numbers[11] = 1.0;
            se_gizmo_move(ui,scene,vraylib.vec2(50.0,0.0)); se_check(vmath.abs(scene.number(child,6)-1.5)<0.001,"scale gizmo snapping");
            app.limit(3);
            while app.frame() {
                target.begin(); app.clear(vraylib.BLUE);
                clip :: vraylib.Clip := app.clip(se_rect(0.0,0.0,32.0,64.0));
                clip.begin(); try { vraylib.draw_rectangle(0,0,64,64,vraylib.RED); } finally { clip.end(); }
                target.end();
                left :: vraylib.Color := se_sample(target,8,32); right :: vraylib.Color := se_sample(target,48,32);
                se_check(left.r == 230 && right.b == 241,"managed panel clip restricts pixels");
                target.begin(); app.clear(vraylib.BLACK); camera.begin();
                try { model.draw_matrix(matrix,vraylib.RED); } finally { camera.end(); }
                target.end();
                pixel :: vraylib.Color := se_sample(target,32,32);
                se_check(pixel.r == 230 && pixel.g == 41 && pixel.b == 55,"matrix model drawing renders transformed instance");
                miss :: vraylib.RayCollision := model.pick(ray,vraylib.math_matrix_identity());
                se_check(!miss.hit,"drawing restores shared model transform");
                app.begin(); target.present(float64(app.width()),float64(app.height())); app.end();
                se_check(app.passes() == 0,"scene passes balanced");
            }
            failed :: Bool := false;
            try { model.draw_matrix(matrix,vraylib.WHITE); } catch (error) { failed = true; }
            se_check(failed,"matrix drawing rejects missing frame/camera");
            failed = false;
            try { camera.ray_viewport(vraylib.vec2(0.0,0.0),0,64); } catch (error) { failed = true; }
            se_check(failed,"viewport dimension validation");
            failed = false;
            try { se_asset(bank,"missing-scene-asset.obj"); } catch (error) { failed = true; }
            se_check(failed && bank.state[1] == 1,"failed asset recorded");
            try { se_asset(bank,"missing-scene-asset.obj"); } catch (error) {}
            se_check(bank.state[1] == 1,"failed asset is not reloaded every frame");
            se_assets_retry(bank); se_check(bank.state[0] == 0,"refresh recycles failed asset slots");
        } finally { app.close(); }
        pool_free(data);
    };
};
se_check(vraylib.live_handles() == 0,"scene graphics cleanup");
out("VMGE_SCENE_RENDER_OK");
