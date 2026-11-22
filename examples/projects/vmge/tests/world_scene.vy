use "../src/editor/world_tools.vy";
use native vmem;
fn world_scene_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
before :: Int64 := vmem.total_allocated();
@slab<Int64,256,512> region world_scene_owner {
    names :: Array := []; paths :: Array := []; i :: Int64 := 0;
    while i < 384 { n :: Array<Int64> := slab_alloc(64); p :: Array<Int64> := slab_alloc(256); names.push(n); paths.push(p); i = i+1; }
    authored :: SEScene := se_scene(names,paths,0); runtime :: SEScene := se_scene(names,paths,128); staging :: SEScene := se_scene(names,paths,256);
    tools :: SEWorldTools := se_world_tools();
    @ring<Float64,17,2048> region world_numbers {
        numbers :: Array := []; i = 0; while i < 17 { a :: Array<Float64> := ring_alloc(2048); numbers.push(a); i = i+1; }
        @ring<Int64,17,41988> region world_texts {
            texts :: Array := []; i = 0; while i < 17 { a :: Array<Int64> := ring_alloc(41988); texts.push(a); i = i+1; }
            history :: SEHistory := se_history(numbers,texts);
            se_demo(authored); se_history_reset(history,authored,true);
            tools.state[0] = 1; se_world_stroke_begin(tools,authored.world);
            i = 0; while i < 300 { se_world_paint(authored.world,float64(i),0.5,0.0,1.0,i%8); tools.state[2] = 1; i = i+1; }
            se_world_stroke_end(tools,authored,history);
            world_scene_check(authored.count() == 5 && authored.world.count() == 300 && history.state[1] == 1,"one undo per many-cell stroke");
            se_undo(history,authored); world_scene_check(authored.world.count() == 0 && !history.dirty(),"undo map stroke");
            se_redo(history,authored); world_scene_check(authored.world.count() == 300,"redo map stroke");
            tools.numbers[12] = 400.0; tools.numbers[13] = 1.0; tools.numbers[17] = 1.0;
            se_world_stroke_begin(tools,authored.world); se_world_dab(tools,authored.world);
            tools.numbers[12] = 406.0; se_world_dab(tools,authored.world); se_world_stroke_end(tools,authored,history);
            world_scene_check(authored.world.count() == 307 && se_world_cell(authored.world,403.0,0.5,1.0) >= 0,"drag interpolation fills skipped map cells");
            se_undo(history,authored); world_scene_check(authored.world.count() == 300,"one undo for interpolated stroke");
            hit_ray :: vraylib.Ray := vraylib.Ray(vraylib.vec3(0.0,10.0,0.0),vraylib.vec3(0.0,-1.0,0.0)); tools.state[4] = 1;
            world_scene_check(se_world_tool_hit(tools,authored.world,hit_ray) && tools.numbers[10] == 1.5,"surface construction uses hit normal to reach adjacent cell"); tools.state[4] = 0;
            imported :: VMGEMap := vmge_map_create(256); i = 0; while i < 256 { imported.append(float64(i),1.0,5.0,2.0,i%8); i = i+1; }
            se_copy(staging,authored); se_import_map(staging,imported);
            world_scene_check(staging.count() == 5 && staging.world.count() == 256 && staging.world.data[19] == 2.0,"legacy import beyond node limit preserves scene nodes");
            se_world_preview(tools,authored.world); world_scene_check(authored.world.side() == 0 && tools.staging.side() == 32,"preview isolates authored terrain");
            tools.state[5] = 0; world_scene_check(authored.world.side() == 0,"cancel preview");
            se_world_preview(tools,authored.world); se_world_preview_apply(tools,authored,history);
            h :: Float64 := se_terrain_height(authored.world,4.0,4.0);
            se_world_stroke_begin(tools,authored.world); se_terrain_sculpt(authored.world,4.0,4.0,3.0,2.0,0.0,0); tools.state[2] = 1; se_world_stroke_end(tools,authored,history);
            se_undo(history,authored); world_scene_check(se_terrain_height(authored.world,4.0,4.0) == h,"undo sculpt"); se_redo(history,authored);
            se_world_stroke_begin(tools,authored.world); se_world_erase(authored.world,0.0,0.5,0.0); se_world_stroke_cancel(tools,authored.world); world_scene_check(authored.world.count() == 300,"real stroke rollback");
            text :: String := se_scene_text(authored); se_import_text(runtime,staging,text);
            world_scene_check(runtime.count() == 5 && runtime.world.count() == 300 && runtime.world.side() == 32 && se_scene_text(runtime) == text,"version 2 roundtrip");
            disk_path :: String := vmge_option("VYNE_VMGE_WORLD_TEST_SCENE","build/world-dev-roundtrip.vscene");
            se_save(authored,disk_path); se_load(runtime,staging,disk_path);
            world_scene_check(se_scene_text(runtime) == text,"resource disk roundtrip");
            failed :: Bool := false;
            try { se_import_text(authored,staging,"VMGE_SCENE|2\nW|2|1|0|0|0|42|0\nC|0|0|0|1|0\nH|0|0|2\nH|0|0|3\n"); } catch (error) { failed = true; }
            world_scene_check(failed && se_scene_text(authored) == text,"failed import leaves authoring bit-for-bit unchanged");
            world_scene_check(vfs.write(disk_path+".bad","VMGE_SCENE|2\nW|2|1|0|0|0|42|0\nC|1|1|1|1|0\nH|0|0|2\n"),"write failed-import fixture");
            failed = false; try { se_load(authored,staging,disk_path+".bad"); } catch (error) { failed = true; }
            world_scene_check(failed && se_scene_text(authored) == text,"failed disk import leaves authoring unchanged");
            se_demo(staging); legacy :: String := se_scene_text(staging); se_import_text(runtime,staging,legacy);
            world_scene_check(runtime.count() == 5 && runtime.world.count() == 0 && runtime.world.side() == 0,"version 1 replaces old resource");
            app :: vraylib.App := vraylib.session(""); app.limit(64);
            player :: VMGEPlayer := vmge_player(app); matrices :: Array<Float64> := []; matrices.reserve(2048); matrices.resize(2048,0.0); cache :: SETransforms := se_transforms(matrices);
            warm :: Int64 := vmem.total_allocated(); commit :: Int64 := vmem.commit_total_allocated();
            while app.frame() {
                app.transient_errors(true);
                actor :: Int64 := se_play_start(authored,runtime,cache,player);
                se_terrain_sculpt(runtime.world,4.0,4.0,3.0,1.0,0.0,0); se_world_erase(runtime.world,0.0,0.5,0.0);
                se_play_update(runtime,cache,player,actor,0.0,0.0,false,false,false,0.02);
                world_scene_check(authored.world.count() == 300 && se_scene_text(authored) == text,"repeated Play preserves authoring world");
                se_clear(runtime);
                test_actor :: Int64 := runtime.spawn(4,"Terrain test actor",-1); se_update_transforms(runtime,cache);
                se_terrain_generate(runtime.world,1,1.0,0.0,42,3);
                runtime.world.data[20496] = 0.0; runtime.world.data[20497] = 2.0; runtime.world.data[20561] = 4.0; runtime.world.data[20562] = 10.0;
                player.camera.set_position(vraylib.vec3(0.25,3.31,0.25)); player.state[1] = -1.0; player.state[2] = 0.0;
                se_play_update(runtime,cache,player,test_actor,0.0,0.0,false,false,false,0.02);
                eye :: vraylib.Vector3 := player.camera.position(); world_scene_check(vmath.abs(eye.y-3.3) < 0.000001 && player.grounded(),"character lands on exact triangle interpolation");
                world_scene_check(se_body_blocked(runtime,cache,vraylib.vec3(0.25,2.8,0.25),1.8,-1),"terrain blocks body below surface");
                se_world_clear(runtime.world); se_world_paint(runtime.world,0.0,2.5,0.0,1.0,0);
                player.camera.set_position(vraylib.vec3(0.85,4.8,0.0)); player.state[1] = 0.0; player.state[2] = 1.0;
                world_scene_check(!se_body_blocked(runtime,cache,vraylib.vec3(0.85,4.8,0.0),1.8,-1),"map support inset is not a horizontal wall");
                se_play_update(runtime,cache,player,test_actor,0.0,0.0,false,false,false,0.02);
                eye = player.camera.position(); world_scene_check(vmath.abs(eye.y-4.8) < 0.000001 && player.grounded(),"map edge support uses the same body width as side collision");
                app.transient_errors(false);
            }
            app.close();
            world_scene_check(vmem.total_allocated()-warm <= 512 && vmem.commit_total_allocated() == commit,"world frame storage bounded and zero commit drift");
            out("WORLD_SCENE_MEMORY headers=",vmem.total_allocated()-warm," commit_drift=",vmem.commit_total_allocated()-commit);
            se_clear(runtime); se_terrain_generate(runtime.world,64,1.0,0.0,42,3); se_world_rectangle(runtime.world,0,0,0,63,63,1.0,0.5,3,false);
            @bounded<67108864> region maximum_resource_roundtrip {
                maximum :: String := se_scene_text(runtime); se_parse(staging,maximum);
                world_scene_check(staging.world.count() == 4096 && staging.world.side() == 64,"maximum-size world persistence within explicit import bound");
            };
            se_world_stroke_begin(tools,runtime.world); failed = false;
            try { se_world_paint(runtime.world,1000.0,0.5,1000.0,1.0,0); } catch (error) { failed = true; se_world_stroke_cancel(tools,runtime.world); }
            world_scene_check(failed && runtime.world.count() == 4096 && se_world_cell(runtime.world,0.0,0.5,0.0) == 0,"capacity failure restores real stroke staging");
        };
    };
};
world_scene_check(vmem.total_allocated() == before,"world owners reclaimed");
out("VMGE_WORLD_SCENE_OK");
