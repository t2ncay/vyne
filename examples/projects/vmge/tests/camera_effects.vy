use "../src/scene/camera_effects.vy";
use "../src/scene/io.vy";
use "../src/engine/input.vy";
use native vmem;
fn camera_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
committed :: Int64 := vmem.commit_total_allocated();
app :: vraylib.App := vmge_window("VMGE camera effect acceptance"); app.limit(80);
try {
    @bounded<134217728> region camera_test_owner {
        names :: Array := []; paths :: Array := []; asset_paths :: Array := []; i :: Int64 := 0;
        while i < 128 { names.push(ss_text(63)); paths.push(ss_text(255)); i = i+1; } i = 0; while i < 64 { asset_paths.push(ss_text(255)); i = i+1; }
        scene :: SEScene := se_scene(names,paths,0); cache :: SETransforms := se_transforms(ss_floats(2048));
        bank :: SEAssets := se_assets(app,asset_paths,vmge_project_root()); effects :: SECameraEffects := se_camera_effects();
        image :: vraylib.Image := app.image(1,1,vraylib.WHITE); texture :: vraylib.Texture := app.texture(image); image.close(); model :: vraylib.Model := app.cube(texture);
        cubes :: Array := [model,model,model,model,model,model,model,model];
        work :: Array<Int64> := ss_ints(128);
        cube :: Int64 := scene.spawn(1,"HDR cube",-1); sky :: Int64 := scene.spawn(5,"Sky",-1); sun :: Int64 := scene.spawn(6,"Sun",-1);
        scene.property(sun,4,180.0); se_extra_set(scene,sun,0,3.0); se_update_transforms(scene,cache);
        camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0,0.0,4.0),vraylib.vec3(0.0,0.0,0.0),60.0); camera.set_orthographic(4.0);
        root :: String := vmge_project_root(); resource :: String := "shaders/camera/vhs.vpipeline";
        camera_check(se_camera_effects_prepare(app,effects,root,resource,64,48),"load VHS ledger and candidate banks");
        se_environment_prepare(scene,cache,bank);
        warm :: Int64 := vmem.total_allocated();
        while app.frame() {
            app.transient_errors(true);
            tick :: Int64 := app.frames()+1;
            if tick == 2 {
                old :: Int64 := effects.state[1]; bytes :: Int64 := effects.state[3]; handles :: Int64 := app.resources();
                camera_check(se_camera_effects_prepare(app,effects,root,resource,2048,2048) && effects.state[1] == old && effects.state[3] == bytes && app.resources() == handles,"GPU peak budget failure retains old bank without leaks");
            }
            if tick == 3 {
                old :: Int64 := effects.state[1]; handles :: Int64 := app.resources();
                vfs.write_atomic("build/vmge-tests/camera-invalid.vshader","VMGE_SHADER|1\nF|2376657273696f6e203333300a696e76616c69640a\nV|\nFP|\nVP|\n");
                path_bytes :: Array<Int64> := ss_text(2048); se_text_write(path_bytes,vfs.join(vraylib.get_working_directory(),"build/vmge-tests/camera-invalid.vshader"));
                vfs.write_atomic("build/vmge-tests/camera-invalid.vpipeline","VMGE_RENDER_PIPELINE|1\nVMGE_PIPELINE|1\nP|10|3|-1|-2|1|0|0|1|1|0\nD|3|"+se_text_hex(path_bytes)+"\n");
                camera_check(se_camera_effects_prepare(app,effects,root,vfs.join(vraylib.get_working_directory(),"build/vmge-tests/camera-invalid.vpipeline"),64,48) && effects.state[1] == old && app.resources() == handles,"failed shader rebuild keeps last working programs and targets");
            }
            if tick == 4 { camera_check(se_camera_effects_prepare(app,effects,root,resource,80,60) && vraylib.RenderTarget(effects.state[1],app.token).width() == 80,"resize publishes matching input/pass bank"); }
            if tick == 5 { se_camera_effect_set(effects,3,1,0,0.0); }
            if tick == 6 { ss_pipeline_enable(effects.pipeline,10,false,false); ss_pipeline_enable(effects.pipeline,20,false,false); se_camera_effects_prepare(app,effects,root,resource,80,60); }
            if tick == 7 { ss_pipeline_enable(effects.pipeline,10,true,false); ss_pipeline_enable(effects.pipeline,20,true,false); se_camera_effects_prepare(app,effects,root,resource,80,60); }
            if tick == 8 {
                scene.meta[(cube%128)*8+4] = 0; scene.world.data[1] = 1.0; scene.world.data[2] = 2.0; scene.world.data[3] = -1.0; scene.world.data[5] = -1.0;
                scene.property(sun,3,90.0); scene.property(sun,4,0.0); se_update_transforms(scene,cache);
                camera.set_view(vraylib.vec3(0.0,3.0,3.0),vraylib.vec3(0.0,0.0,0.0));
            }
            if tick == 9 {
                se_world_clear(scene.world); scene.meta[(cube%128)*8+4] = 1; scene.meta[(sun%128)*8+4] = 0; se_extra_set(scene,sky,10,0.0);
                lamp :: Int64 := scene.spawn(7,"Point test",-1); scene.property(lamp,2,2.0); se_extra_set(scene,lamp,0,8.0); se_extra_set(scene,lamp,1,5.0);
                se_update_transforms(scene,cache); camera.set_view(vraylib.vec3(0.0,0.0,4.0),vraylib.vec3(0.0,0.0,0.0));
            }
            if tick == 10 {
                se_delete(scene,scene.handle(3),work); scene.meta[(sun%128)*8+4] = 1; scene.property(sun,3,0.0); scene.property(sun,4,180.0); se_extra_set(scene,sky,10,0.15); se_update_transforms(scene,cache);
            }
            if tick == 11 {
                old :: Int64 := effects.state[1]; handles :: Int64 := app.resources(); source :: Array<Int64> := ss_text(8191);
                se_text_write(source,"#version 330\nin vec2 fragTexCoord;out vec4 finalColor;uniform sampler2D inputTexture;uniform vec2 seTime;void main()\{finalColor=texture(inputTexture,fragTexCoord)+vec4(seTime,0.0,0.0);}\n");
                vfs.write_atomic("build/vmge-tests/camera-invalid.vshader","VMGE_SHADER|1\nF|"+se_text_hex(source)+"\nV|\nFP|\nVP|\n");
                se_camera_effects_reload(effects);
                camera_check(se_camera_effects_prepare(app,effects,root,vfs.join(vraylib.get_working_directory(),"build/vmge-tests/camera-invalid.vpipeline"),80,60) && effects.state[1] == old && app.resources() == handles,"valid GLSL with wrong engine-uniform type is rejected before camera publication");
            }
            if tick == 12 { se_camera_effects_reload(effects); camera_check(se_camera_effects_prepare(app,effects,root,resource,80,60) && effects.log[0] == 0,"explicit retry publishes repaired camera resources"); }
            if tick == 13 {
                build :: vraylib.ShaderBuild := app.compile_shader(null,"#version 330\nin vec2 fragTexCoord;out vec4 finalColor;uniform sampler2D inputTexture;void main()\{finalColor=texture(inputTexture,fragTexCoord);}\n");
                camera_check(build.success,"external reference probe shader compiles");
                old :: Int64 := effects.state[1]; build.shader.set_texture("inputTexture",vraylib.RenderTarget(effects.pipeline.outputs[1],app.token).texture());
                blocked :: Bool := false; try { se_camera_effects_close(effects); } catch (error) { blocked = true; }
                camera_check(blocked && effects.state[1] == old && vraylib.RenderTarget(old,app.token).valid(),"referenced camera bank cannot partially close");
                build.shader.set_texture("inputTexture",null); build.shader.close();
            }
            capture :: vraylib.RenderTarget := vraylib.RenderTarget(effects.state[1],app.token); se_scene_capture(app,scene,cache,bank,cubes,camera,capture);
            raw :: Array<Float64> := capture.read_pixel(capture.width()/2,capture.height()/2);
            if tick == 8 { camera_check(raw[1] > 1.0,"directional light illuminates immediate terrain triangles with HDR headroom"); }
            else { camera_check(raw[0] > 1.0,"directional/point-lit HDR camera scene retains highlights before tonemapping"); }
            output :: vraylib.RenderTarget := se_camera_effects_run(app,effects); pixel :: Array<Float64> := output.read_pixel(output.width()/2,output.height()/2);
            if tick == 6 { camera_check(output.token == capture.token && vmath.abs(pixel[0]-raw[0]) < 0.0001,"all-pass bypass returns exact raw camera output"); }
            else { camera_check(pixel[0] >= 0.0 && pixel[0] <= 1.0 && pixel[3] > 0.99,"VHS + tonemap output is finite, opaque and display ranged"); }
            camera_check(app.passes() == 0,"camera and fullscreen passes balance");
            app.begin(); try { output.present(float64(app.width()),float64(app.height())); } finally { app.end(); }
            app.transient_errors(false);
        }
        out("CAMERA_MEMORY headers=",vmem.total_allocated()-warm," commit_drift=",vmem.commit_total_allocated()-committed);
        camera_check(vmem.total_allocated()-warm <= 4096 && vmem.commit_total_allocated() == committed,"repeated camera rendering/reloads have bounded retained headers and zero commit drift");
        se_camera_effects_close(effects); se_programs_close(bank.programs);
    };
} finally { app.close(); }
camera_check(vraylib.live_handles() == 0,"all camera GPU handles released");
out("VMGE_CAMERA_EFFECTS_OK");
