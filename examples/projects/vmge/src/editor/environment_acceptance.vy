use "workspace_host.vy";
use "workspace.vy";
use "../scene/camera_effects.vy";

# Isolated integration flow. Enabled explicitly by the acceptance runner; every
# scene/pipeline/layout write goes to build-owned paths, never shipped assets.
fn se_environment_host_check(ok :: Bool,message :: String) -> Bool { if !ok { throw "Environment host: "+message; } return true; }
fn se_environment_host_autotest(app :: vraylib.App,host :: SEWorkspaceHost,ui :: SEUI,scene :: SEScene,runtime :: SEScene,staging :: SEScene,cache :: SETransforms,player :: VMGEPlayer,camera :: vraylib.Camera,effects :: SECameraEffects,history :: SEHistory,bank :: SEAssets,path :: Array<Int64>) -> Bool {
    tick :: Int64 := app.frames()+1; prefix :: String := vmge_option("VYNE_VMGE_ENVIRONMENT_PREFIX","build/vmge-tests/environment-flow");
    if tick == 1 { se_demo(scene); se_history_reset(history,scene,true); ui.state[0] = -1; se_node_picker_open(ui); ui.state[48] = 8; }
    if tick == 2 { se_environment_host_check(ui.state[47] == 1 && ui.state[39] == 1,"picker exclusively captures frame input"); ui.state[50] = 1; }
    if tick == 3 {
        se_environment_host_check(scene.count() == 7 && ui.state[47] == 0,"queued picker creates Daylight preset");
        se_undo(history,scene); se_environment_host_check(scene.count() == 5,"preset undo removes both nodes"); se_redo(history,scene);
        cam :: Int64 := scene.handle(4); se_make_active(scene,cam); se_text_write(scene.paths[scene.offset+4],"shaders/camera/vhs.vpipeline");
        lamp :: Int64 := scene.spawn(7,"Lamp",-1); scene.property(lamp,0,2.0); scene.property(lamp,1,2.0); scene.property(lamp,2,4.0);
        se_record(history,scene); ui.state[0] = cam; ui.state[53] = 1;
    }
    if tick == 5 {
        se_environment_host_check(effects.state[1] != 0 && effects.pipeline.state[0] == 2 && effects.log[0] == 0,"authored camera preview prepares VHS/tonemap");
        output :: vraylib.RenderTarget := vraylib.RenderTarget(effects.pipeline.outputs[1],app.token); pixel :: Array<Float64> := output.read_pixel(output.width()/2,output.height()/2);
        se_environment_host_check(pixel[0] >= 0.0 && pixel[0] <= 1.0 && pixel[3] > 0.99,"integrated camera produces display-range pixels");
    }
    if tick == 6 { se_toggle_play(ui,scene,runtime,cache,player,app); }
    if tick == 7 { se_environment_host_check(runtime.count() == 8 && runtime.settings[0] == 4 && runtime.settings[1] == 5,"Play copies environment and active selections"); }
    if tick == 8 { se_extra_set(runtime,runtime.handle(4),0,0.0); }
    if tick == 9 { se_environment_host_check(scene.extra[4*16] == 1.0 && runtime.extra[4*16] == 0.0,"runtime bypass does not mutate authoring"); }
    if tick == 10 { se_toggle_play(ui,scene,runtime,cache,player,app); }
    if tick == 11 {
        raw := vfs.read(vfs.join(bank.root,"shaders/camera/vhs.vpipeline.host"));
        if raw == null || !vfs.write_atomic(prefix+".vpipeline.host",raw) { throw "Cannot stage camera pipeline ledger"; }
        full :: String := prefix+".vpipeline"; if !vraylib.is_path_absolute(full) { full = vfs.join(vraylib.get_working_directory(),full); }
        se_text_write(scene.paths[scene.offset+4],full); se_record(history,scene); ui.state[52] = 1;
    }
    if tick == 13 { se_environment_host_check(host.workspace.layout.state[0] == 3 && host.workspace.state[56] != 0,"Rendering workspace consumes full authored camera capture"); }
    if tick == 14 { vraylib.set_window_size(900,600); }
    if tick == 16 { se_environment_host_check(se_host_pipeline_save(host,bank),"Rendering saves portable pass ledger to build-owned path"); ui.state[54] = host.workspace.pipeline.tokens[0]; }
    if tick == 17 { se_environment_host_check(ui.state[54] == host.workspace.pipeline.tokens[0],"unchanged camera extent reuses prepared Rendering targets"); host.workspace.layout.state[0] = 0; ui.state[53] = 1; se_camera_effects_reload(effects); }
    if tick == 19 { se_environment_host_check(effects.log[0] == 0 && vraylib.RenderTarget(effects.state[1],app.token).width() > 0,"saved camera pipeline reloads after resize and workspace navigation"); }
    if tick == 20 {
        sun :: Int64 := scene.handle(6); se_extra_set(scene,sun,0,2.0); se_record(history,scene); se_undo(history,scene);
        se_environment_host_check(scene.extra[6*16] == 1.0,"light edit undo"); se_redo(history,scene);
    }
    if tick == 21 { se_text_write(path,prefix+".vscene"); se_world_save_scene(scene,se_text_read(path)); se_world_load_scene(scene,staging,se_text_read(path)); se_history_reset(history,scene,true); ui.state[0] = scene.handle(4); }
    if tick == 24 {
        se_environment_host_check(scene.count() == 8 && scene.settings[0] == 4 && scene.settings[1] == 5 && scene.extra[6*16] == 2.0 && !history.dirty(),"v4 save/reopen retains environment, camera and clean revision");
        se_environment_host_check(app.passes() == 0 && ui.state[21] == 0,"integrated capture and presentation stay balanced");
        out("VMGE_ENVIRONMENT_HOST_OK");
    } return true;
}
