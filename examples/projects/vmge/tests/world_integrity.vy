use "../src/engine/world_resource_io.vy";
use "../src/scene/history.vy";
use native vmem;

fn world_integrity_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
fn world_integrity_snapshot(scene :: SEScene,meta :: Array,values :: Array,clocks :: Array) -> Bool {
    m :: Array<Int64> := meta; v :: Array<Float64> := values; c :: Array<Int64> := clocks;
    i :: Int64 := 0; while i < 1024 { m[i] = scene.meta[i]; i = i+1; }
    i = 0; while i < 2048 { v[i] = scene.values[i]; i = i+1; }
    i = 0; while i < 128 { c[i] = scene.clocks[i]; i = i+1; } return true;
}
fn world_integrity_unchanged(scene :: SEScene,meta :: Array,values :: Array,clocks :: Array,count :: Int64) -> Bool {
    m :: Array<Int64> := meta; v :: Array<Float64> := values; c :: Array<Int64> := clocks;
    world_integrity_check(scene.count() == count,"failed duplicate changed node count");
    i :: Int64 := 0; while i < 1024 { world_integrity_check(m[i] == scene.meta[i],"failed duplicate changed metadata"); i = i+1; }
    i = 0; while i < 2048 { world_integrity_check(v[i] == scene.values[i],"failed duplicate changed numbers"); i = i+1; }
    i = 0; while i < 128 { world_integrity_check(c[i] == scene.clocks[i],"failed duplicate consumed a generation"); i = i+1; } return true;
}

before :: Int64 := vmem.total_allocated();
@slab<Int64,256,512> region scene_integrity_owner {
    names :: Array := []; paths :: Array := []; i :: Int64 := 0;
    while i < 384 { n :: Array<Int64> := slab_alloc(64); p :: Array<Int64> := slab_alloc(256); names.push(n); paths.push(p); i = i+1; }
    authored :: SEScene := se_scene(names,paths,0); runtime :: SEScene := se_scene(names,paths,128); staging :: SEScene := se_scene(names,paths,256);
    work :: Array<Int64> := []; work.reserve(128); work.resize(128,0);
    snapshot_meta :: Array<Int64> := []; snapshot_meta.reserve(1024); snapshot_meta.resize(1024,0);
    snapshot_values :: Array<Float64> := []; snapshot_values.reserve(2048); snapshot_values.resize(2048,0.0);
    snapshot_clocks :: Array<Int64> := []; snapshot_clocks.reserve(128); snapshot_clocks.resize(128,0);
    @ring<Float64,17,2048> region integrity_numeric {
        numbers :: Array := []; i = 0; while i < 17 { a :: Array<Float64> := ring_alloc(2048); numbers.push(a); i = i+1; }
        @ring<Int64,17,41988> region integrity_text {
            texts :: Array := []; i = 0; while i < 17 { a :: Array<Int64> := ring_alloc(41988); texts.push(a); i = i+1; }
            history :: SEHistory := se_history(numbers,texts);

            # A copied generation must not be issued again in a fresh destination.
            i = 0; while i < 5 { discarded :: Int64 := staging.spawn(0,"Generation",-1); se_delete(staging,discarded,work); i = i+1; }
            copied :: Int64 := staging.spawn(0,"Copied",-1); se_copy(runtime,staging);
            world_integrity_check(runtime.clocks[0] >= copied/128,"copy did not reserve source generation");
            se_delete(runtime,copied,work); replacement :: Int64 := runtime.spawn(0,"Replacement",-1);
            world_integrity_check(replacement > copied && !runtime.valid(copied),"copy/delete/spawn revived copied handle");
            se_copy(runtime,staging); highwater :: Int64 := runtime.clocks[0]; se_clear(runtime);
            after_clear :: Int64 := runtime.spawn(0,"After clear",-1);
            world_integrity_check(after_clear/128 > highwater && !runtime.valid(copied) && !runtime.valid(replacement),"copy of older generation lowered high-water mark");
            se_replace(runtime,staging); replaced :: Int64 := runtime.handle(0);
            world_integrity_check(replaced > after_clear && !runtime.valid(after_clear) && !runtime.valid(copied),"replacement reused an issued generation");
            se_history_reset(history,runtime,true); se_delete(runtime,replaced,work); se_record(history,runtime);
            future :: Int64 := runtime.spawn(0,"Future",-1); se_record(history,runtime); se_undo(history,runtime);
            branch :: Int64 := runtime.spawn(0,"Undo branch",-1);
            world_integrity_check(branch > future && !runtime.valid(future),"undo/delete state reused future handle");
            se_undo(history,runtime); highwater = runtime.clocks[0]; se_clear(runtime);
            after_undo_clear :: Int64 := runtime.spawn(0,"Undo clear",-1);
            world_integrity_check(after_undo_clear/128 > highwater && !runtime.valid(replaced),"clear after undo lowered generation high-water");
            saved_clock :: Int64 := runtime.clocks[0]; runtime.clocks[0] = 72057594037927935;
            world_integrity_snapshot(runtime,snapshot_meta,snapshot_values,snapshot_clocks); exhausted :: Bool := false;
            try { se_replace(runtime,staging); } catch (error) { exhausted = true; }
            world_integrity_check(exhausted,"replacement wrapped an exhausted generation"); world_integrity_unchanged(runtime,snapshot_meta,snapshot_values,snapshot_clocks,1); runtime.clocks[0] = saved_clock;

            # A later bad descendant must reject before root/child clones exist.
            root :: Int64 := authored.spawn(0,"Root 🎮",-1); child :: Int64 := authored.spawn(1,"Əşya",root); leaf :: Int64 := authored.spawn(1,"草地",child);
            authored.meta[(root%128)*8+7] = 17; authored.meta[(child%128)*8+7] = 42; authored.meta[(leaf%128)*8+7] = 93;
            authored.meta[(child%128)*8+6] = 12; se_text_write(authored.paths[authored.offset+leaf%128],"assets/草地.obj");
            se_history_reset(history,authored,true); world_integrity_snapshot(authored,snapshot_meta,snapshot_values,snapshot_clocks);
            leaf_name :: Array<Int64> := authored.names[authored.offset+leaf%128]; saved_lead :: Int64 := leaf_name[1]; leaf_name[1] = 192;
            failed :: Bool := false;
            try { se_duplicate(authored,root,work); se_record(history,authored); } catch (error) { failed = true; }
            leaf_name[1] = saved_lead;
            world_integrity_check(failed && history.state[1] == 0,"invalid descendant did not fail before history record");
            world_integrity_unchanged(authored,snapshot_meta,snapshot_values,snapshot_clocks,3);
            saved_length :: Int64 := leaf_name[0]; leaf_name[0] = leaf_name.size(); failed = false;
            try { se_duplicate(authored,root,work); } catch (error) { failed = true; } leaf_name[0] = saved_length;
            world_integrity_check(failed,"bad source text capacity accepted"); world_integrity_unchanged(authored,snapshot_meta,snapshot_values,snapshot_clocks,3);
            authored.meta[(root%128)*8+2] = leaf; failed = false;
            try { se_duplicate(authored,root,work); } catch (error) { failed = true; } authored.meta[(root%128)*8+2] = -1;
            world_integrity_check(failed,"cyclic duplicate hierarchy accepted"); world_integrity_unchanged(authored,snapshot_meta,snapshot_values,snapshot_clocks,3);
            clone :: Int64 := se_duplicate(authored,root,work); clone_child :: Int64 := work[child%128]; clone_leaf :: Int64 := work[leaf%128];
            world_integrity_check(authored.count() == 6 && authored.parent(clone_child) == clone && authored.parent(clone_leaf) == clone_child,"duplicate lost subtree parent mapping");
            world_integrity_check(authored.meta[(clone%128)*8+7] == 17 && authored.meta[(clone_child%128)*8+7] == 42 && authored.meta[(clone_leaf%128)*8+7] == 93 && authored.meta[(clone_child%128)*8+6] == 12,"duplicate lost flags or asset metadata");
            world_integrity_check(authored.name(clone) == "Root 🎮" && authored.name(clone_child) == "Əşya" && authored.name(clone_leaf) == "草地" && authored.path(clone_leaf) == "assets/草地.obj","raw duplication lost UTF-8 bytes");
            se_delete(authored,clone,work); fresh :: Int64 := authored.spawn(0,"Fresh",-1);
            world_integrity_check(!authored.valid(clone) && authored.meta[(fresh%128)*8+7] == 0 && authored.meta[(fresh%128)*8+6] == -1,"spawn inherited old clone flags/assets"); se_delete(authored,fresh,work);
            while authored.count() < 127 { authored.spawn(0,"Filler",-1); }
            world_integrity_snapshot(authored,snapshot_meta,snapshot_values,snapshot_clocks); failed = false;
            try { se_duplicate(authored,root,work); } catch (error) { failed = true; }
            world_integrity_check(failed,"subtree capacity preflight failed"); world_integrity_unchanged(authored,snapshot_meta,snapshot_values,snapshot_clocks,127);

            # Both save entry points must preserve precise node-only v1 numbers.
            se_clear(authored); node :: Int64 := authored.spawn(1,"Precise node",-1);
            authored.property(node,0,123456.789); authored.property(node,1,4.9406564584124654e-324); authored.property(node,2,-1e-300); authored.property(node,6,1.123456789012345);
            exact :: String := se_scene_text(authored); rows :: Array := vmge_split(exact,"\n");
            world_integrity_check(rows[0] == "VMGE_SCENE|1" && se_world_scene_text(authored) == exact,"node-only precision changed the v1 field layout");
            se_parse(staging,exact); parsed :: Int64 := staging.handle(0);
            world_integrity_check(staging.number(parsed,0) == 123456.789 && staging.number(parsed,1) == 4.9406564584124654e-324 && staging.number(parsed,2) == -1e-300 && staging.number(parsed,6) == 1.123456789012345,"node-only text roundtrip lost Float64 precision");
            prefix :: String := "build/world-review-node"; override := vraylib.environment("VYNE_VMGE_WORLD_REVIEW_PREFIX"); if override != null && override != "" { prefix = override; }
            se_save(authored,prefix+".vscene"); se_load(runtime,staging,prefix+".vscene");
            world_integrity_check(se_scene_text(runtime) == exact,"basic v1 disk save/load lost precision");
            se_world_save_scene(authored,prefix+"-extended.vscene"); se_world_load_scene(runtime,staging,prefix+"-extended.vscene");
            world_integrity_check(se_scene_text(runtime) == exact,"extended node-only disk save/load lost precision");
            authored.values[(node%128)*16] = 3000000.0; failed = false;
            try { se_save(authored,prefix+".vscene"); } catch (error) { failed = true; } authored.property(node,0,123456.789);
            world_integrity_check(failed && vfs.read(prefix+".vscene") == exact,"failed numeric serialization modified saved file");
            failed = false; try { se_save(authored,"build"); } catch (error) { failed = true; }
            world_integrity_check(failed && vfs.is_dir("build") && vfs.read(prefix+".vscene") == exact,"basic atomic save failure was not propagated");
            failed = false; try { se_world_save_scene(authored,"build"); } catch (error) { failed = true; }
            world_integrity_check(failed && vfs.is_dir("build") && vfs.read(prefix+"-extended.vscene") == exact,"extended atomic save failure was not propagated");
        };
    };
};
world_integrity_check(vmem.total_allocated() == before,"integrity owners not reclaimed"); out("VMGE_WORLD_INTEGRITY_OK");
