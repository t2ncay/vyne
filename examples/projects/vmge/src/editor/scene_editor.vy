use "workspace.vy";
use "world_tools.vy";
use "document.vy";
use "../engine/input.vy";

fn se_editor_close_request(app :: vraylib.App,ui :: SEUI,tools :: SEWorldTools,scene :: SEScene,history :: SEHistory,path :: Array<Int64>,document :: SEDocument) -> Bool {
    se_world_stroke_cancel(tools,scene.world); tools.state[5] = 0;
    se_world_settings_cancel(tools.settings);
    if ui.state[2] != -1 && ui.numbers[17] != 0.0 { se_record(history,scene); }
    ui.state[2] = -1; ui.state[10] = 0; app.capture_mouse(false);
    se_document_cancel(ui,document);
    return se_document_request(ui,scene,history,path,document,3,"");
}

fn se_editor_commit(ui :: SEUI,scene :: SEScene,staging :: SEScene,history :: SEHistory,bank :: SEAssets,directory :: Array<Int64>,path :: Array<Int64>,document :: SEDocument) -> Bool {
    if ui.state[1] == 16 && ui.state[13] == 1 {
        if history.dirty() && ui.state[34] == 0 && ui.state[41] == 0 {
            value :: String := se_text_read(ui.text);
            se_document_request(ui,scene,history,path,document,2,value); return false;
        }
        value :: String := se_text_read(ui.text);
        se_world_load_scene(scene,staging,value); se_text_write(path,value);
        se_history_reset(history,scene,true); ui.state[0] = -1; ui.state[36] = 1; ui.state[38] = 1;
        ui.state[2] = -1; ui.state[6] = -1; ui.state[7] = 0;
        ui.state[1] = -1; ui.state[13] = 0; ui.state[41] = 0; return true;
    }
    if ui.state[1] == 16 && ui.state[13] == 2 {
        value :: String := se_text_read(ui.text);
        se_world_save_scene(scene,value); se_text_write(path,value); history.saved(); ui.state[36] = 1;
        ui.state[1] = -1; ui.state[13] = 0;
        if ui.state[34] != 0 { se_document_finish(ui,scene,history,path,document); }
        return true;
    }
    if ui.state[1] == 16 && (ui.state[13] == 4 || ui.state[13] == 5) {
        if ui.state[13] == 4 { se_world_export_obj(scene.world,se_text_read(ui.text)); }
        else { se_world_save_map(scene.world,se_text_read(ui.text)); }
        ui.state[1] = -1; ui.state[13] = 0; return true;
    }
    if ui.state[1] == 16 && ui.state[13] == 3 {
        @bounded<16777216> region resource_map_import {
            imported :: VMGEMap := vmge_map_load(se_text_read(ui.text),4096,false);
            se_copy(staging,scene); se_import_map(staging,imported); se_replace(scene,staging);
        };
        se_record(history,scene); ui.state[1] = -1; ui.state[13] = 0; ui.state[0] = -2; return true;
    }
    return se_commit_edit(ui,scene,staging,history,bank,directory,path);
}

fn se_toggle_play(ui :: SEUI,authored :: SEScene,runtime :: SEScene,cache :: SETransforms,player :: VMGEPlayer,app :: vraylib.App) -> Bool {
    if ui.state[4] == 0 {
        ui.state[5] = se_play_start(authored,runtime,cache,player);
        ui.state[4] = 1; ui.state[20] = ui.state[20]+1;
    } else {
        ui.state[4] = 0; ui.state[5] = -1; ui.state[10] = 0;
        se_clear(runtime); app.capture_mouse(false);
    }
    ui.state[1] = -1; ui.state[2] = -1; ui.state[6] = -1;
    return true;
}
fn se_toolbar(app :: vraylib.App,ui :: SEUI,authored :: SEScene,runtime :: SEScene,cache :: SETransforms,player :: VMGEPlayer,history :: SEHistory,scene_path :: Array<Int64>,width :: Int64,document :: SEDocument) -> Bool {
    vraylib.draw_rectangle(0,0,width,42,vraylib.rgba(23,27,34,255));
    editable :: Bool := ui.state[4] == 0 && ui.state[13] == 0;
    if se_button("New",se_rect(6.0,6.0,46.0,29.0),editable) { se_document_request(ui,authored,history,scene_path,document,1,""); }
    if se_button("Open",se_rect(56.0,6.0,46.0,29.0),editable) { se_document_request(ui,authored,history,scene_path,document,2,""); }
    if se_button("Save",se_rect(106.0,6.0,46.0,29.0),editable) { se_document_save(ui,authored,history,scene_path,document,false); }
    if se_button("Undo",se_rect(156.0,6.0,46.0,29.0),editable) { se_undo(history,authored); }
    if se_button("Redo",se_rect(206.0,6.0,46.0,29.0),editable) { se_redo(history,authored); }
    label :: String := "Play"; if ui.state[4] != 0 { label = "Stop"; }
    if se_button(label,se_rect(260.0,6.0,48.0,29.0),ui.state[13] == 0 && ui.state[2] == -1 && ui.state[1] == -1) { se_toggle_play(ui,authored,runtime,cache,player,app); }
    mode :: Int64 := 0;
    labels := ["T","R","S"];
    while mode < 3 { if se_button(labels[mode],se_rect(318.0+float64(mode)*31.0,6.0,27.0,29.0),editable && ui.state[2] == -1) { ui.state[3] = mode; } mode = mode + 1; }
    if se_button("Snap",se_rect(419.0,6.0,47.0,29.0),editable) { ui.numbers[16] = 1.0-ui.numbers[16]; }
    if se_button("Focus",se_rect(472.0,6.0,48.0,29.0),editable && authored.valid(ui.state[0])) { se_focus(ui,cache,ui.state[0]%128); }
    status :: String := "VMGE 3D"; if history.dirty() { status = status+" *"; }
    se_draw_text(status,530,13,16,vraylib.SKYBLUE);
    if width >= 780 {
        if se_button("Save As",se_rect(float64(width)-156.0,6.0,82.0,29.0),editable) { se_document_save(ui,authored,history,scene_path,document,true); }
        if se_button("Exit",se_rect(float64(width)-68.0,6.0,60.0,29.0),editable) { se_document_request(ui,authored,history,scene_path,document,3,""); }
    }
    return true;
}
fn se_shortcuts(app :: vraylib.App,ui :: SEUI,authored :: SEScene,runtime :: SEScene,cache :: SETransforms,player :: VMGEPlayer,history :: SEHistory,work :: Array<Int64>,scene_path :: Array<Int64>,document :: SEDocument) -> Bool {
    if ui.state[1] != -1 || ui.state[13] != 0 { return false; }
    if vraylib.is_key_pressed(vraylib.KEY_F6) && ui.state[2] == -1 { se_toggle_play(ui,authored,runtime,cache,player,app); }
    if ui.state[4] != 0 { return true; }
    control :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL) || vraylib.is_key_down(vraylib.KEY_RIGHT_CONTROL);
    shift :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT) || vraylib.is_key_down(vraylib.KEY_RIGHT_SHIFT);
    if control {
        if vraylib.is_key_pressed(vraylib.KEY_Z) { se_undo(history,authored); }
        if vraylib.is_key_pressed(vraylib.KEY_Y) { se_redo(history,authored); }
        if vraylib.is_key_pressed(vraylib.KEY_S) { se_document_save(ui,authored,history,scene_path,document,shift); }
        if vraylib.is_key_pressed(vraylib.KEY_O) { se_document_request(ui,authored,history,scene_path,document,2,""); }
        if vraylib.is_key_pressed(vraylib.KEY_N) { se_document_request(ui,authored,history,scene_path,document,1,""); }
        if vraylib.is_key_pressed(vraylib.KEY_E) {
            ext :: String := "world.obj"; ui.state[13] = 4;
            if shift { ext = "world.dat"; ui.state[13] = 5; }
            se_edit_begin(ui,16,-1,vfs.join(vfs.dirname(se_text_read(scene_path)),ext));
        }
        if vraylib.is_key_pressed(vraylib.KEY_I) { se_edit_begin(ui,16,-1,vmge_option("VYNE_VMGE_MAP","tau_map.dat")); ui.state[13] = 3; }
    }
    if ui.state[13] != 0 || ui.state[35] != 0 { return true; }
    if authored.valid(ui.state[0]) {
        if vraylib.is_key_pressed(vraylib.KEY_DELETE) { se_delete(authored,ui.state[0],work); ui.state[0] = -1; se_record(history,authored); }
        if control && vraylib.is_key_pressed(vraylib.KEY_D) { ui.state[0] = se_duplicate(authored,ui.state[0],work); se_record(history,authored); }
        if vraylib.is_key_pressed(vraylib.KEY_F) { se_focus(ui,cache,ui.state[0]%128); }
    }
    return true;
}
fn se_autotest(app :: vraylib.App,ui :: SEUI,authored :: SEScene,runtime :: SEScene,cache :: SETransforms,player :: VMGEPlayer,history :: SEHistory,bank :: SEAssets,work :: Array<Int64>,directory :: Array<Int64>,scene_path :: Array<Int64>,document :: SEDocument) -> Bool {
    tick :: Int64 := app.frames()+1;
    crate :: Int64 := authored.handle(2);
    if tick == 1 { ui.state[0] = crate; }
    if tick == 2 { authored.property(crate,0,3.0); se_record(history,authored); }
    if tick == 3 { se_edit_begin(ui,14,crate,"Edited Crate"); se_commit_edit(ui,authored,runtime,history,bank,directory,scene_path); }
    if tick == 4 { ui.state[0] = se_duplicate(authored,crate,work); se_record(history,authored); }
    if tick == 5 { authored.reparent(ui.state[0],authored.handle(3)); se_record(history,authored); }
    if tick == 6 { se_delete(authored,ui.state[0],work); ui.state[0] = crate; se_record(history,authored); }
    if tick == 7 { se_undo(history,authored); }
    if tick == 8 { se_redo(history,authored); }
    if tick == 9 { se_edit_begin(ui,16,-1,se_text_read(scene_path)); ui.state[13] = 2; se_editor_commit(ui,authored,runtime,history,bank,directory,scene_path,document); }
    if tick == 10 || tick == 12 { se_toggle_play(ui,authored,runtime,cache,player,app); }
    if tick == 14 { vraylib.set_window_size(900,600); }
    return true;
}
fn se_studio_check(ok :: Bool,message :: String) -> Bool { if !ok { throw "Studio: "+message; } return true; }
fn se_studio_autotest(app :: vraylib.App,ui :: SEUI,tools :: SEWorldTools,authored :: SEScene,runtime :: SEScene,staging :: SEScene,cache :: SETransforms,player :: VMGEPlayer,history :: SEHistory,bank :: SEAssets,directory :: Array<Int64>,path :: Array<Int64>,document :: SEDocument) -> Bool {
    tick :: Int64 := app.frames()+1;
    prefix :: String := vmge_option("VYNE_VMGE_STUDIO_PREFIX","build/studio-dev-flow");
    if tick == 1 {
        se_demo(authored); se_history_reset(history,authored,false); ui.state[36] = 0;
        tools.numbers[6] = 4.0; tools.numbers[24] = 2.0; tools.numbers[7] = 1.5;
        tools.numbers[21] = 0.125; tools.numbers[22] = 3.0; tools.numbers[23] = 0.625; tools.numbers[25] = 3.0;
        se_world_preview(tools,authored.world); se_world_preview_apply(tools,authored,history);
        se_world_paint_data(authored.world.data,20.0,0.5,20.0,1.0,2); se_record(history,authored);
    }
    if tick == 2 {
        ui.state[0] = authored.handle(2); authored.property(ui.state[0],0,3.0);
        se_edit_begin(ui,14,ui.state[0],"Edited Crate"); se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
    }
    if tick == 3 {
        se_edit_begin(ui,14,authored.handle(2),"Inspector sentinel");
        se_world_settings_begin(tools.settings,tools.numbers); se_edit_begin(tools.settings.editor,0,-1,"77");
    }
    if tick == 4 {
        se_studio_check(ui.state[39] == 1 && ui.state[42] == 1 && ui.state[40] == 1,"modal dispatched once before global input");
        se_studio_check(se_text_read(ui.text) == "Inspector sentinel" && authored.name(authored.handle(2)) == "Edited Crate","modal leaves scene focus and authoring untouched");
        se_world_settings_cancel(tools.settings);
        se_studio_check(ui.state[39] == 1 && ui.state[42] == 1,"closed modal still owns entire frame");
    }
    if tick == 5 {
        se_document_cancel(ui,document);
        se_world_stroke_begin(tools,authored.world); se_world_paint_data(authored.world.data,22.0,0.5,20.0,1.0,3);
        tools.state[2] = 1; se_world_stroke_end(tools,authored,history);
    }
    if tick == 6 { se_undo(history,authored); se_studio_check(authored.world.count() == 1,"stroke undo"); se_redo(history,authored); }
    if tick == 7 {
        se_document_request(ui,authored,history,path,document,1,"");
        se_document_choice(ui,authored,history,path,document,1);
        se_edit_begin(ui,16,-1,prefix+"-absent/failed.vscene"); ui.state[13] = 2;
        failed :: Bool := false; try { se_editor_commit(ui,authored,staging,history,bank,directory,path,document); } catch (error) { failed = true; }
        se_studio_check(failed && history.dirty() && ui.state[34] == 1 && authored.count() == 5,"failed save preserves dirty document and pending New");
        se_document_cancel(ui,document); se_studio_check(authored.count() == 5 && ui.state[34] == 0,"cancel pending action without mutation");
    }
    if tick == 8 {
        se_edit_begin(ui,16,-1,se_text_read(path)); ui.state[13] = 2;
        se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
        se_studio_check(!history.dirty(),"extended save cleans only on success");
    }
    if tick == 9 {
        tools.numbers[21] = 0.5;
        se_edit_begin(ui,16,-1,se_text_read(path)); ui.state[13] = 1;
        se_editor_commit(ui,authored,staging,history,bank,directory,path,document); se_editor_reload(ui,tools,authored.world);
        se_studio_check(authored.world.data[9] == 1.0 && authored.world.data[10] == 0.125 && tools.numbers[21] == 0.125 && tools.numbers[23] == 0.625,"G metadata roundtrip through editor commit and tools reload");
    }
    if tick == 10 { se_toggle_play(ui,authored,runtime,cache,player,app); }
    if tick == 11 { se_play_update(runtime,cache,player,ui.state[5],0.1,0.1,false,true,false,0.016); se_studio_check(player.height() == 0.9 && authored.number(authored.handle(2),0) == 3.0,"Play crouch and authoring isolation"); }
    if tick == 12 { se_toggle_play(ui,authored,runtime,cache,player,app); }
    if tick == 13 {
        se_edit_begin(ui,16,-1,prefix+".obj"); ui.state[13] = 4; se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
        se_edit_begin(ui,16,-1,prefix+".dat"); ui.state[13] = 5; se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
        se_studio_check(!history.dirty(),"map-only exports do not dirty scene");
    }
    if tick == 14 { vraylib.set_window_size(900,600); }
    if tick == 15 {
        se_document_request(ui,authored,history,path,document,1,"");
        # Clean New intentionally executes; reload the saved extended document.
        se_studio_check(authored.count() == 1 && history.dirty(),"New creates fresh dirty document");
        se_document_request(ui,authored,history,path,document,2,se_text_read(path));
        se_document_choice(ui,authored,history,path,document,0);
        se_studio_check(authored.count() == 1 && history.dirty(),"cancel Open preserves new document");
        se_document_request(ui,authored,history,path,document,2,se_text_read(path));
        se_document_choice(ui,authored,history,path,document,2);
        se_editor_commit(ui,authored,staging,history,bank,directory,path,document); se_editor_reload(ui,tools,authored.world);
    }
    if tick == 16 {
        se_studio_check(authored.count() == 5 && authored.world.count() == 2 && authored.world.side() == 4 && ui.state[40] == 1 && !history.dirty(),"final integrated state");
        out("VMGE_STUDIO_AUTOTEST_OK");
    }
    return true;
}

fn se_scene_editor() -> Int64 {
    app :: vraylib.App := vraylib.App(0);
    frames :: Int64 := 0; node_count :: Int64 := 0; cycles :: Int64 := 0; drift :: Int64 := 0; commit_drift :: Int64 := 0;
    schema :: Array := se_schema();
    try {
        project :: String := vmge_project_root();
        if !vraylib.is_path_absolute(project) { project = vfs.join(vraylib.get_working_directory(),project); }
        app = vmge_window("VMGE - 3D Scene Editor");
        app.defer_close(true);
        se_theme_load(app,project);
        vraylib.set_window_min_size(640,360);
        vraylib.set_exit_key(0);
        app.capture_mouse(false);
        @slab<Int64,256,512> region editor_session {
            names :: Array := []; paths :: Array := []; asset_paths :: Array := [];
            i :: Int64 := 0;
            while i < 384 {
                name_bytes :: Array<Int64> := slab_alloc(64); names.push(name_bytes);
                path_bytes :: Array<Int64> := slab_alloc(256); paths.push(path_bytes); i = i + 1;
            }
            i = 0; while i < 64 { path_bytes :: Array<Int64> := slab_alloc(256); asset_paths.push(path_bytes); i = i + 1; }
            edit_bytes :: Array<Int64> := slab_alloc(256);
            file_bytes :: Array<Int64> := slab_alloc(256);
            directory_bytes :: Array<Int64> := slab_alloc(256);
            authored :: SEScene := se_scene(names,paths,0);
            runtime :: SEScene := se_scene(names,paths,128);
            staging :: SEScene := se_scene(names,paths,256);
            ui :: SEUI := se_ui(edit_bytes);
            passive :: SEUI := se_editor_passive(ui);
            document :: SEDocument := se_document();
            world_tools :: SEWorldTools := se_world_tools();
            se_text_write(file_bytes,vmge_option("VYNE_VMGE_SCENE",vfs.join(project,"scene.vscene")));
            se_text_write(directory_bytes,project);
            work :: Array<Int64> := []; work.reserve(128); work.resize(128,0);
            @pool<Float64,2048> region transform_work {
                author_matrices :: Array<Float64> := pool_alloc();
                play_matrices :: Array<Float64> := pool_alloc();
                author_cache :: SETransforms := se_transforms(author_matrices);
                play_cache :: SETransforms := se_transforms(play_matrices);
                @ring<Float64,17,2048> region undo_numeric {
                    numeric_slots :: Array := []; i = 0;
                    while i < 17 { slot :: Array<Float64> := ring_alloc(2048); numeric_slots.push(slot); i = i + 1; }
                    @ring<Int64,17,41988> region undo_text {
                        text_slots :: Array := []; i = 0;
                        while i < 17 { slot :: Array<Int64> := ring_alloc(41988); text_slots.push(slot); i = i + 1; }
                        history :: SEHistory := se_history(numeric_slots,text_slots);
                        @ring<Int64,32,256> region output_history {
                            logs :: Array := []; i = 0;
                            while i < 32 { slot :: Array<Int64> := ring_alloc(256); logs.push(slot); i = i + 1; }
                            log_state :: Array<Int64> := [0];
                            projection_buffer :: Array<Float64> := [0.0,0.0,0.0,0.0];
                            try {
                                se_demo(authored);
                                if vmge_setting("VYNE_VMGE_WORLD_DEMO",0) != 0 { se_resource_demo(authored); }
                                input :: String := se_text_read(file_bytes);
                                loaded :: Bool := false;
                                if input != "" && vfs.exists(input) { se_world_load_scene(authored,staging,input); loaded = true; }
                                ui.state[36] = 0; if loaded { ui.state[36] = 1; }
                                if !loaded { out("Scene path absent: starting demo; Save will create ",input); }
                                se_world_tools_load_options(world_tools,authored.world);
                                se_history_reset(history,authored,loaded);
                                se_ui_prepare(ui,directory_bytes);
                                textures :: Array := vmge_palette(app,vmge_asset_root(project));
                                cubes :: Array := vmge_models(app,textures);
                                app.assets(project);
                                bank :: SEAssets := se_assets(app,asset_paths,project);
                                camera :: vraylib.Camera := app.camera(vraylib.vec3(8.0,6.0,12.0),vraylib.vec3(0.0,1.0,2.0),70.0);
                                player :: VMGEPlayer := vmge_player(app);
                                target :: vraylib.RenderTarget := app.target(640,360);
                                automated :: Bool := vmge_setting("VYNE_VMGE_AUTOTEST",0) != 0;
                                se_message(logs,log_state,"Scene editor ready. F6 Play/Stop; Ctrl+S Save; right-drag orbit.");
                                warm :: Int64 := vmem.total_allocated();
                                commit_warm :: Int64 := vmem.commit_total_allocated();
                                while app.frame() {
                                    app.transient_errors(true);
                                    try {
                                    viewport :: vraylib.Rectangle := se_viewport(ui,app.width(),app.height());
                                    projection_buffer[0] = float64(viewport.x); projection_buffer[1] = float64(viewport.y); projection_buffer[2] = float64(viewport.width); projection_buffer[3] = float64(viewport.height);
                                    if !se_valid_viewport(projection_buffer[0:4]) { throw "Invalid editor viewport dimensions"; }
                                     try {
                                          if app.close_requested() { se_editor_close_request(app,ui,world_tools,authored,history,file_bytes,document); }
                                           if se_editor_input(ui,world_tools,document) {
                                             se_editor_commit(ui,authored,staging,history,bank,directory_bytes,file_bytes,document);
                                             se_message(logs,log_state,"Edit applied");
                                         }
                                         se_editor_reload(ui,world_tools,authored.world);
                                        se_update_transforms(authored,author_cache);
                                        se_update_asset_bounds(authored,author_cache,bank,cubes);
                                         if ui.state[39] == 0 && ui.state[43] == 0 && !se_world_tools_busy(world_tools) && ui.state[2] == -1 { se_shortcuts(app,ui,authored,runtime,play_cache,player,history,work,file_bytes,document); }
                                         if automated {
                                             if vmge_setting("VYNE_VMGE_STUDIO_AUTOTEST",0) == 1 { se_studio_autotest(app,ui,world_tools,authored,runtime,staging,play_cache,player,history,bank,directory_bytes,file_bytes,document); }
                                             else { se_autotest(app,ui,authored,runtime,play_cache,player,history,bank,work,directory_bytes,file_bytes,document); }
                                         }
                                         world_consumed_escape :: Bool := false;
                                         if ui.state[4] == 0 && ui.state[39] == 0 && ui.state[35] == 0 && ui.state[13] == 0 && ui.state[2] == -1 {
                                            world_consumed_escape = vraylib.is_key_pressed(vraylib.KEY_ESCAPE) && (world_tools.state[1] != 0 || world_tools.state[5] != 0);
                                            try { se_world_tools_input(world_tools,ui,authored,history,camera,viewport); }
                                            catch (error) { se_world_stroke_cancel(world_tools,authored.world); throw error; }
                                        }
                                         if ui.state[39] == 0 && ui.state[43] == 0 && !world_consumed_escape && !se_world_tools_busy(world_tools) && ui.state[35] == 0 && ui.state[13] == 0 && ui.state[1] == -1 && vraylib.is_key_pressed(vraylib.KEY_ESCAPE) {
                                             if ui.state[10] != 0 { ui.state[10] = 0; app.capture_mouse(false); }
                                             else { se_document_request(ui,authored,history,file_bytes,document,3,""); }
                                         }
                                         if ui.state[37] != 0 { app.stop(); }
                                         if ui.state[39] == 0 && ui.state[43] == 0 && ui.state[35] == 0 && ui.state[13] == 0 && !se_world_tools_busy(world_tools) { se_splitters(ui,app.width(),app.height(),viewport); }
                                        if ui.state[4] == 0 {
                                            se_update_transforms(authored,author_cache);
                                            se_update_asset_bounds(authored,author_cache,bank,cubes);
                                             background_input :: Bool := ui.state[39] == 0 && ui.state[43] == 0 && ui.state[35] == 0 && ui.state[13] == 0 && !se_world_tools_busy(world_tools);
                                             if background_input { se_orbit(ui,camera,viewport); }
                                             if ui.state[2] != -1 && ui.state[39] == 0 {
                                                if vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) { se_gizmo_move(ui,authored,vraylib.get_mouse_position()); }
                                                else { if ui.numbers[17] != 0.0 { se_record(history,authored); } ui.state[2] = -1; }
                                             } else { if background_input && world_tools.state[0] == 0 {
                                                 mouse_position :: vraylib.Vector2 := vraylib.get_mouse_position();
                                                 if se_hover(viewport) && !se_view_controls_hit(ui,viewport) && mouse_position.y > viewport.y+114.0 && ui.state[1] == -1 && ui.state[13] == 0 && ui.state[16] == 0 && ui.state[17] == 0 && ui.state[18] == 0 && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) {
                                                    axis :: Int64 := se_gizmo_pick(ui,authored,author_cache,camera,viewport,projection_buffer);
                                                    if axis != -1 { se_gizmo_begin(ui,authored,author_cache,camera,viewport,axis); }
                                                    else {
                                                        mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
                                                        ray :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(mouse.x-viewport.x,mouse.y-viewport.y),int64(viewport.width),int64(viewport.height));
                                                         ui.state[0] = se_pick(authored,author_cache,bank,cubes,ray);
                                                         world_tools.state[6] = -2; if ui.state[0] == -2 { world_tools.state[6] = int64(authored.world.work[4232]); }
                                                    }
                                                }
                                            } }
                                            se_update_transforms(authored,author_cache);
                                            se_update_asset_bounds(authored,author_cache,bank,cubes);
                                         } else { if ui.state[39] == 0 && ui.state[35] == 0 && ui.state[13] == 0 {
                                            se_update_asset_bounds(runtime,play_cache,bank,cubes);
                                            if se_hover(viewport) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) { ui.state[10] = 1; app.capture_mouse(true); }
                                            if ui.state[10] != 0 { player.camera.look(); }
                                            forward :: Float64 := 0.0; right :: Float64 := 0.0;
                                            if ui.state[10] != 0 { forward = vmge_forward(); right = vmge_right(); }
                                            if automated { forward = 0.1; }
                                            se_play_update(runtime,play_cache,player,ui.state[5],forward,right,vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT),vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL),vraylib.is_key_pressed(vraylib.KEY_SPACE),app.dt());
                                            se_update_asset_bounds(runtime,play_cache,bank,cubes);
                                         } }
                                    } catch (error) {
                                        ui.state[21] = ui.state[21]+1; se_message(logs,log_state,string(error));
                                        if automated { throw error; }
                                    }
                                    viewport = se_viewport(ui,app.width(),app.height());
                                    vw :: Int64 := int64(viewport.width); vh :: Int64 := int64(viewport.height);
                                    if target.width() != vw || target.height() != vh { target.resize(vw,vh); }
                                    target.begin();
                                    try {
                                        app.clear(vraylib.rgba(43,48,58,255));
                                        active_camera :: vraylib.Camera := camera; if ui.state[4] != 0 { active_camera = player.camera; }
                                        active_camera.begin();
                                        try {
                                            vraylib.draw_grid(40,1.0);
                                            if ui.state[4] == 0 {
                                                render_world :: SEWorld := authored.world; if world_tools.state[5] != 0 { render_world = world_tools.staging; }
                                                se_draw_scene_resource(authored,author_cache,bank,cubes,true,ui.state[0],render_world);
                                                if world_tools.state[0] == 0 { se_gizmo_draw(ui,authored,author_cache); }
                                                se_world_tools_draw(world_tools,authored.world);
                                            }
                                            else { se_draw_scene(runtime,play_cache,bank,cubes,false,-1); }
                                        } finally { active_camera.end(); }
                                    } finally { target.end(); }
                                    app.begin();
                                    try {
                                        app.clear(vraylib.rgba(19,23,29,255));
                                        target.texture().draw_region(se_rect(0.0,0.0,float64(vw),float64(vh)),viewport,vraylib.vec2(0.0,0.0),0.0,vraylib.WHITE);
                                        try {
                                             panels :: SEUI := ui;
                                             if ui.state[39] != 0 || ui.state[43] != 0 || ui.state[35] != 0 || ui.state[13] != 0 || ui.state[2] != -1 || se_world_tools_busy(world_tools) {
                                                 se_editor_freeze(ui,passive); panels = passive;
                                             }
                                             se_toolbar(app,panels,authored,runtime,play_cache,player,history,file_bytes,app.width(),document);
                                             world_panel :: SEUI := ui;
                                             if ui.state[39] != 0 || ui.state[35] != 0 || ui.state[13] != 0 || ui.state[2] != -1 { se_editor_freeze(ui,passive); world_panel = passive; }
                                             se_world_tools_panel(app,world_tools,world_panel,authored,history,viewport);
                                             # A toolbar/world button may have opened a dialog this frame.
                                             if ui.state[35] != 0 || ui.state[13] != 0 || se_world_tools_busy(world_tools) { se_editor_freeze(ui,passive); panels = passive; }
                                             se_view_controls(panels,camera,viewport);
                                             se_tree(app,panels,authored,history,work,se_rect(0.0,42.0,viewport.x-4.0,viewport.height+8.0));
                                              se_inspector(app,panels,authored,history,schema,se_rect(viewport.x+viewport.width+4.0,42.0,float64(app.width())-viewport.x-viewport.width-4.0,viewport.height+8.0));
                                              if ui.state[44] != 0 && !se_world_tools_busy(world_tools) { ui.state[44] = 0; se_world_settings_begin(world_tools.settings,world_tools.numbers); se_editor_freeze(ui,passive); panels = passive; }
                                             panel_directory :: Array<Int64> := directory_bytes;
                                             if panels.state[13] == -1 { panel_directory = ui.asset_directory; }
                                             se_asset_panel(app,panels,authored,history,bank,panel_directory,file_bytes,se_rect(viewport.x,viewport.y+viewport.height+4.0,float64(app.width())-viewport.x,float64(app.height())-viewport.y-viewport.height-4.0));
                                            se_output(app,logs,log_state,se_rect(0.0,viewport.y+viewport.height+4.0,viewport.x-4.0,float64(app.height())-viewport.y-viewport.height-4.0));
                                            if ui.state[6] != -1 { se_draw_text("Choose new parent in Scene tree",int64(viewport.x+12.0),int64(viewport.y+72.0),18,vraylib.YELLOW); }
                                            if ui.state[4] != 0 { se_draw_text("PLAY - click viewport to capture mouse; Escape releases",int64(viewport.x+12.0),int64(viewport.y+12.0),16,vraylib.YELLOW); }
                                             se_ui_overlays(panels);
                                             if ui.state[13] > 0 && ui.state[42] == 0 && !se_world_tools_modal_open(world_tools) {
                                                 if ui.state[13] == 4 || ui.state[13] == 5 { se_draw_text("Export resource map cells only (no terrain or node meshes)",20,app.height()-20,14,vraylib.YELLOW); }
                                                 if se_file_dialog(ui,se_rect(vmath.max(10.0,float64(app.width())*0.1),float64(app.height())*0.3,float64(app.width())*0.8,120.0)) { se_editor_commit(ui,authored,staging,history,bank,directory_bytes,file_bytes,document); }
                                             }
                                             if ui.state[42] == 0 { se_document_dialog(app,ui,authored,history,file_bytes,document); }
                                             se_world_tools_modal_draw(app,world_tools);
                                             se_editor_reload(ui,world_tools,authored.world);
                                        } catch (error) { ui.state[21] = ui.state[21]+1; se_message(logs,log_state,string(error)); if automated { throw error; } }
                                    } finally { app.end(); }
                                    screenshot :: String := vmge_option("VYNE_VMGE_SCREENSHOT","");
                                    if automated && app.frames() == 14 && screenshot != "" { vraylib.take_screenshot(screenshot); }
                                    if bank.state[1] != ui.state[25] { ui.state[25] = bank.state[1]; se_message(logs,log_state,"Missing asset: magenta bounds shown; edit its path to recover"); }
                                    } finally { app.transient_errors(false); }
                                }
                                frames = app.frames(); node_count = authored.count(); cycles = ui.state[20];
                                drift = vmem.total_allocated()-warm; commit_drift = vmem.commit_total_allocated()-commit_warm;
                                out("VMGE_SCENE_EDITOR_STATE nodes=",node_count," play_cycles=",cycles," errors=",ui.state[21]," transient_headers=",drift," commit_drift=",commit_drift);
                                if automated {
                                    if node_count != 5 || authored.name(authored.handle(2)) != "Edited Crate" || authored.number(authored.handle(2),0) != 3.0 || cycles != 1 || commit_drift != 0 { throw "Scene editor acceptance state mismatch"; }
                                }
                            } finally { app.close(); }
                        };
                    };
                };
                pool_free(author_matrices); pool_free(play_matrices);
            };
        };
    } finally { app.close(); }
    out("VMGE_SCENE_EDITOR_COMPLETE frames=",frames," live_handles=",vraylib.live_handles());
    return frames;
}
