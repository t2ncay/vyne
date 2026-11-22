use "../src/editor/workspace.vy";
use "../src/scene/camera_effects.vy";
fn env_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
before :: Int64 := vmem.total_allocated(); committed :: Int64 := vmem.commit_total_allocated();
@slab<Int64,256,512> region environment_owner {
    names :: Array := []; paths :: Array := []; i :: Int64 := 0;
    while i < 384 { names.push(slab_alloc(64)); paths.push(slab_alloc(256)); i = i+1; }
    scene :: SEScene := se_scene(names,paths,0); staging :: SEScene := se_scene(names,paths,128); runtime :: SEScene := se_scene(names,paths,256);
    cache :: SETransforms := se_transforms(ss_floats(2048)); lights :: Array<Float64> := ss_floats(48); work :: Array<Int64> := ss_ints(128);
    ui :: SEUI := se_ui(ss_text(255)); app :: vraylib.App := vraylib.session(""); camera :: vraylib.Camera := app.camera(vraylib.vec3(3.0,4.0,5.0),vraylib.vec3(0.0,1.0,0.0),70.0);
    @ring<Float64,17,2048> region environment_numeric_history {
        numbers :: Array := []; i = 0; while i < 17 { numbers.push(ring_alloc(2048)); i = i+1; }
        @ring<Int64,17,41988> region environment_text_history {
            texts :: Array := []; i = 0; while i < 17 { texts.push(ring_alloc(41988)); i = i+1; }
            history :: SEHistory := se_history(numbers,texts);
            se_demo(scene); se_update_transforms(scene,cache); se_history_reset(history,scene,true);
            se_node_picker_open(ui); ui.state[48] = 8; sky :: Int64 := se_create_catalog_node(scene,ui,camera,cache); se_record(history,scene);
            sun :: Int64 := scene.handle(int64(scene.extra[(sky%128)*16+9]));
            env_check(scene.count() == 7 && scene.kind(sky) == 5 && scene.kind(sun) == 6,"daylight preset creates linked sky and sun");
            env_check(se_undo(history,scene) && scene.count() == 5 && !history.dirty(),"preset is one undo step"); se_redo(history,scene);
            env_check(se_active_node(scene,5) == sky && scene.extra[(sky%128)*16+9] == float64(sun%128),"redo restores active sky and sun link");
            out("ENV_PRESET_OK");
            camera_id :: Int64 := scene.handle(4); se_make_active(scene,camera_id); se_text_write(scene.paths[camera_id%128],"shaders/camera/vhs.vpipeline");
            se_extra_set(scene,sun,0,2.25); se_extra_set(scene,sky,0,0.12345678901234567);
            lamp :: Int64 := scene.spawn(7,"Lamp",-1); scene.property(lamp,0,2.0); se_extra_set(scene,lamp,1,12.5);
            se_update_transforms(scene,cache); se_light_collect(scene,cache,lights);
            out("ENV_LIGHT_COLLECTION_OK");
            env_check(lights[0] == 1.0 && lights[8] == 1.0 && lights[16] == 2.0 && lights[19] == 12.5,"bounded light collection uses world transforms");
            @bounded<16777216> region environment_roundtrip {
                text :: String := se_scene_text(scene); env_check(vraylib.text_subtext(text,0,12) == "VMGE_SCENE|4","extensions select v4"); se_parse(staging,text);
                env_check(staging.extra[(sky%128)*16] == scene.extra[(sky%128)*16] && staging.path(staging.handle(4)) == scene.path(camera_id),"exact environment and camera reference roundtrip");
                failed :: Bool := false; try { se_import_text(runtime,staging,text+"A|-1|-1\n"); } catch (error) { failed = true; }
                env_check(failed && runtime.count() == 0,"duplicate active record rejected before publication");
            };
            se_copy(runtime,scene); se_extra_set(runtime,runtime.handle(sun%128),0,0.5);
            out("ENV_ROUNDTRIP_OK");
            env_check(scene.extra[(sun%128)*16] == 2.25,"Play light properties are independent");
            lighting_root :: Int64 := scene.spawn(0,"Lighting group",-1); scene.reparent(sky,lighting_root); scene.reparent(sun,lighting_root);
            clone :: Int64 := se_duplicate(scene,lighting_root,work); linked_clone :: Int64 := -1; i = 0;
            out("ENV_DUPLICATE_CREATED root=",lighting_root," clone=",clone);
            while i < 128 { if scene.meta[i*8] != 0 && scene.meta[i*8+3] == 5 && scene.parent(scene.handle(i)) == clone { linked_clone = scene.handle(i); } i = i+1; }
            env_check(linked_clone != -1 && scene.extra[(linked_clone%128)*16+9] != float64(sun%128),"duplicate remaps internal sun link");
            se_delete(scene,clone,work); se_delete(scene,sun,work); env_check(scene.extra[(sky%128)*16+9] == -1.0,"deleted sun clears borrowed slot reference");
            env_check(se_node_matches(6,"sun") && se_node_matches(7,"lamp") && !se_picker_match(9,""),"catalog search synonyms and empty result sentinel");
            ui.state[48] = 2; ui.state[49] = 1; camera.set_orthographic(10.0);
            aligned :: Int64 := se_create_catalog_node(scene,ui,camera,cache);
            env_check(scene.number(aligned,0) == 3.0 && scene.number(aligned,1) == 4.0 && scene.number(aligned,2) == 5.0 && scene.number(aligned,13) == 70.0,"new camera aligns position and retains perspective lens from orthographic editor view"); se_delete(scene,aligned,work);
            count_before :: Int64 := scene.count(); ui.state[48] = 1; ui.numbers[6] = 2000000.0;
            placement_failed :: Bool := false; try { se_create_catalog_node(scene,ui,camera,cache); } catch (error) { placement_failed = true; }
            env_check(placement_failed && scene.count() == count_before,"invalid creation placement cannot reserve a partial node"); ui.numbers[6] = 0.0;
            failed :: Bool := false; try { se_extra_set(scene,lamp,1,-2.0); } catch (error) { failed = true; } env_check(failed && scene.extra[(lamp%128)*16+1] == 12.5,"invalid range rejected before mutation");
            app.limit(120); warm :: Int64 := vmem.total_allocated();
            while app.frame() {
                @bounded<2097152> region environment_frame_work {
                    se_extra_set(scene,lamp,0,1.0+float64(app.frames()%2)); se_record(history,scene); se_copy(runtime,scene);
                    se_update_transforms(runtime,cache); se_light_collect(runtime,cache,lights);
                    se_undo(history,scene); se_redo(history,scene); se_node_matches(7,"point");
                };
            }
            env_check(vmem.total_allocated()-warm <= 1024 && vmem.commit_total_allocated() == committed,"frame/history churn has bounded headers and zero commit drift");
        };
    }; app.close();
};
env_check(vmem.total_allocated() == before && vraylib.live_handles() == 0,"owner backing and native handles reclaimed");
out("VMGE_ENVIRONMENT_OK");
