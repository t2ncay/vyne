use "../src/scene/io.vy";
use "../src/scene/history.vy";
use "../src/scene/play.vy";
use native vmem;

fn se_check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
before :: Int64 := vmem.total_allocated();
committed :: Int64 := vmem.commit_total_allocated();
@slab<Int64,256,512> region scene_owner {
    names :: Array := []; paths :: Array := [];
    i :: Int64 := 0;
    while i < 384 {
        name_bytes :: Array<Int64> := slab_alloc(64); names.push(name_bytes);
        path_bytes :: Array<Int64> := slab_alloc(256); paths.push(path_bytes); i = i + 1;
    }
    authored :: SEScene := se_scene(names, paths, 0);
    runtime :: SEScene := se_scene(names, paths, 128);
    staging :: SEScene := se_scene(names, paths, 256);
    work :: Array<Int64> := []; work.resize(128, 0);
    @ring<Float64,17,2048> region numeric_history {
        numbers :: Array := []; i = 0;
        while i < 17 { slot :: Array<Float64> := ring_alloc(2048); numbers.push(slot); i = i + 1; }
        @ring<Int64,17,41988> region text_history {
            texts :: Array := []; i = 0;
            while i < 17 { slot :: Array<Int64> := ring_alloc(41988); texts.push(slot); i = i + 1; }
            history :: SEHistory := se_history(numbers, texts);
            root :: Int64 := authored.spawn(0, "Root", -1);
            cube :: Int64 := authored.spawn(1, "Crate", root);
            se_text_write(authored.names[authored.offset + (cube % 128)], "Azərbaycan 🎮");
            se_check(authored.name(cube) == "Azərbaycan 🎮", "UTF-8 text round trip");
            se_history_reset(history, authored, true);
            authored.property(cube, 0, 3.0); se_record(history, authored);
            se_check(history.dirty() && se_undo(history, authored) && authored.number(cube, 0) == 0.0, "undo and dirty revision");
            se_check(!history.dirty() && se_redo(history, authored) && authored.number(cube, 0) == 3.0, "redo restores owned state");
            se_copy(runtime, authored); runtime.property(cube, 0, 20.0);
            se_check(authored.number(cube, 0) == 3.0, "Play state is independent");
            failed :: Bool := false;
            try { authored.reparent(root, cube); } catch (error) { failed = true; }
            se_check(failed && authored.parent(root) == -1, "reject cycle without mutation");
            clone :: Int64 := se_duplicate(authored, root, work);
            se_check(authored.count() == 4 && authored.parent(clone) == -1, "duplicate hierarchy");
            se_delete(authored, clone, work); se_check(authored.count() == 2 && !authored.valid(clone), "delete subtree invalidates handles");
            text :: String := se_scene_text(authored); se_parse(staging, text);
            se_check(staging.count() == 2 && staging.name(staging.handle(cube % 128)) == "Azərbaycan 🎮", "scene serialization preserves names");
            se_replace(runtime, staging); se_check(runtime.count() == 2, "validated scene replacement");
            app :: vraylib.App := vraylib.session(""); app.limit(200);
            player :: VMGEPlayer := vmge_player(app);
            matrix_data :: Array<Float64> := []; matrix_data.reserve(2048); matrix_data.resize(2048,0.0);
            play_cache :: SETransforms := se_transforms(matrix_data);
            se_demo(staging);
            warm :: Int64 := vmem.total_allocated();
            while app.frame() {
                app.transient_errors(true);
                try {
                    @bounded<4096> region rejected_edit {
                        message :: String := "Rejected edit " + string(app.frames());
                        throw message;
                    };
                } catch (error) { se_check(error == "Rejected edit " + string(app.frames()),"recoverable error survives nested rewind"); }
                authored.property(cube, 0, float64(app.frames()));
                se_text_write(authored.names[authored.offset + (cube % 128)], "Crate " + string(app.frames()));
                se_record(history, authored);
                se_copy(runtime, authored); runtime.property(cube, 0, -20.0);
                actor :: Int64 := se_play_start(staging,runtime,play_cache,player);
                se_play_update(runtime,play_cache,player,actor,0.1,0.0,false,false,false,0.02);
                se_check(staging.number(actor,2) == 0.0 && runtime.number(actor,2) > 0.0,"repeated Play/Stop preserves authoring scene");
                se_clear(runtime);
                temporary :: String := se_scene_text(authored);
                app.transient_errors(false);
            }
            app.close();
            out("SCENE_MEMORY call_headers=", vmem.total_allocated() - warm," commit_drift=",vmem.commit_total_allocated()-committed);
            se_check(vmem.total_allocated() - warm <= 512 && vmem.commit_total_allocated() == committed, "frame churn retains only bounded call headers and no commit drift");
            undos :: Int64 := 0; while se_undo(history, authored) { undos = undos + 1; }
            se_check(undos == 16, "ring eviction bounds history");
            se_check(vraylib.live_handles() == 0, "scene native cleanup");
        };
    };
};
se_check(vmem.total_allocated() == before, "owner regions reclaim backing");
out("VMGE_SCENE_OK");
