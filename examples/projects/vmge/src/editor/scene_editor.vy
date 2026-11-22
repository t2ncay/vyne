use "workspace.vy";
use "world_tools.vy";
use "document.vy";
use "workspace_host.vy";
use "../engine/input.vy";

fn se_editor_close_request(app :: vraylib.App,ui :: SEUI,tools :: SEWorldTools,scene :: SEScene,history :: SEHistory,path :: Array<Int64>,document :: SEDocument) -> Bool {
    se_editor_close_prepare(app,ui,tools,scene,history,document);
    return se_document_request(ui,scene,history,path,document,3,"");
}
fn se_editor_close_prepare(app :: vraylib.App,ui :: SEUI,tools :: SEWorldTools,scene :: SEScene,history :: SEHistory,document :: SEDocument) -> Bool {
    se_world_stroke_cancel(tools,scene.world); tools.state[5] = 0;
    se_world_settings_cancel(tools.settings);
    if ui.state[2] != -1 && ui.numbers[17] != 0.0 { se_record(history,scene); }
    ui.state[2] = -1; ui.state[10] = 0; app.capture_mouse(false);
    se_document_cancel(ui,document);
    return true;
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

fn se_scene_editor() -> Int64 { return se_scene_editor_workspace(0); }
fn se_scene_editor_workspace(initial_workspace :: Int64) -> Int64 {
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
            owner_start :: Int64 := vmem.total_allocated();
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
                                 shader_test :: Bool := vmge_setting("VYNE_VMGE_SHADER_AUTOTEST",0) == 1;
                                  bank :: SEAssets := se_assets(app,asset_paths,project);
                                  host :: SEWorkspaceHost := se_workspace_host(app,project);
                                  se_host_bind_registry(host,bank);
                                  if !shader_test { if vfs.exists(se_text_read(host.pipeline_path)+".host") { se_host_pipeline_load(app,host,bank,se_text_read(host.pipeline_path)); } }
                                  if host.state[13] != 0 { se_host_resolve_editable(host,bank); }
                                 if initial_workspace < 0 || initial_workspace > 3 { throw "Initial workspace must be Scene0 / World1 / Shaders2 / Rendering3"; }
                                  host.workspace.layout.state[0] = initial_workspace;
                                  host.workspace.state[55] = 0; host.workspace.panel.state[14] = 0;
                                  if shader_test { app.limit(32); }
                                 legacy :: Bool := vmge_setting("VYNE_VMGE_AUTOTEST",0) != 0 && !shader_test;
                                 if shader_test {
                                     prefix :: String := vmge_option("VYNE_VMGE_SHADER_PREFIX","build/shader-host-dev-flow");
                                     publications :: String := prefix+"-published"; if !vraylib.is_path_absolute(publications) { publications = vfs.join(vraylib.get_working_directory(),publications); }
                                     if !vfs.mkdir_p(publications) { throw "Cannot create isolated shader host publication directory"; }
                                      se_text_write(host.publish_directory,publications);
                                      se_host_autotest_prepare(app,host,prefix);
                                 }
                                 if legacy { se_text_write(host.workspace.layout_path,"build/shader-host-dev-legacy.layout"); }
                                camera :: vraylib.Camera := app.camera(vraylib.vec3(8.0,6.0,12.0),vraylib.vec3(0.0,1.0,2.0),70.0);
                                player :: VMGEPlayer := vmge_player(app);
                                target :: vraylib.RenderTarget := app.target(640,360);
                                automated :: Bool := vmge_setting("VYNE_VMGE_AUTOTEST",0) != 0;
                                se_message(logs,log_state,"Scene editor ready. F6 Play/Stop; Ctrl+S Save; right-drag orbit.");
                                warm :: Int64 := vmem.total_allocated();
                                 commit_warm :: Int64 := vmem.commit_total_allocated();
                                 owner_bytes :: Int64 := warm-owner_start;
                                  if owner_bytes > 134217728 { throw "Scene/studio preframe owner budget exceeds 128 MiB"; }
                                 if shader_test { out("VMGE_SHADER_HOST_PREFRAME_BYTES ",owner_bytes); }
                                 try { while app.frame() {
                                    app.transient_errors(true);
                                    try {
                                    bounds :: vraylib.Rectangle := se_rect(0.0,0.0,float64(app.width()),float64(app.height()));
                                    ss_theme_dpi(host.workspace.layout.values[3]);
                                    active_tab :: Int64 := host.workspace.layout.state[0];
                                    host.state[14] = 0;
                                    if host.state[0] != 0 || ss_workspace_input_captured(host.workspace) || host.workspace.layout.state[5] != 0 { host.state[14] = 1; }
                                    if host.state[13] != 0 { se_host_resolve_editable(host,bank); }
                                    if host.state[5] == 1 && ui.state[34] == 0 && ui.state[35] == 0 && ui.state[37] == 0 { host.state[5] = 0; }
                                    if host.state[0] == 0 && host.state[6] >= 128 {
                                        se_host_selected_material(host,authored,bank,host.state[6]); host.state[6] = -1;
                                    }
                                    ui.state[46] = int64(!legacy);
                                    viewport :: vraylib.Rectangle := se_host_viewport(host,ui,app.width(),app.height(),legacy);
                                    projection_buffer[0] = float64(viewport.x); projection_buffer[1] = float64(viewport.y); projection_buffer[2] = float64(viewport.width); projection_buffer[3] = float64(viewport.height);
                                    if !se_valid_viewport(projection_buffer[0:4]) { throw "Invalid editor viewport dimensions"; }
                                     try {
                                            if app.close_requested() && host.state[0] == 0 && ui.state[34] == 0 && !ss_workspace_input_captured(host.workspace) {
                                                se_editor_close_prepare(app,ui,world_tools,authored,history,document);
                                               host.state[5] = 0;
                                              if se_host_dirty(host) { host.state[0] = 1; app.capture_mouse(false); }
                                              else { se_editor_close_request(app,ui,world_tools,authored,history,file_bytes,document); }
                                          }
                                           scene_input :: Bool := (active_tab < 2 || ui.state[35] != 0 || ui.state[13] != 0 || se_world_tools_modal_open(world_tools)) && host.state[0] == 0 && host.state[14] == 0;
                                          if scene_input { if se_editor_input(ui,world_tools,document) {
                                             se_editor_commit(ui,authored,staging,history,bank,directory_bytes,file_bytes,document);
                                             se_message(logs,log_state,"Edit applied");
                                          } }
                                          if !scene_input { ui.state[39] = 1; ui.state[42] = 0; ui.state[43] = 0; }
                                         se_editor_reload(ui,world_tools,authored.world);
                                        se_update_transforms(authored,author_cache);
                                         se_update_asset_bounds(authored,author_cache,bank,cubes);
                                           if shader_test {
                                               if vmge_setting("VYNE_VMGE_SHADER_AUTHORING_AUTOTEST",0) == 1 { se_shader_host_authoring_autotest(app,host,ui,authored,runtime,history,bank,cubes,file_bytes,play_cache,player,document); }
                                               else { se_shader_host_autotest(app,host,ui,authored,runtime,staging,history,bank,cubes,file_bytes,play_cache,player,document); }
                                           }
                                         if ui.state[39] == 0 && ui.state[43] == 0 && !se_world_tools_busy(world_tools) && ui.state[2] == -1 { se_shortcuts(app,ui,authored,runtime,play_cache,player,history,work,file_bytes,document); }
                                         if automated {
                                             if vmge_setting("VYNE_VMGE_STUDIO_AUTOTEST",0) == 1 { se_studio_autotest(app,ui,world_tools,authored,runtime,staging,play_cache,player,history,bank,directory_bytes,file_bytes,document); }
                                             else { se_autotest(app,ui,authored,runtime,play_cache,player,history,bank,work,directory_bytes,file_bytes,document); }
                                         }
                                         world_consumed_escape :: Bool := false;
                                         if ui.state[4] == 0 && ui.state[39] == 0 && ui.state[35] == 0 && ui.state[13] == 0 && ui.state[2] == -1 {
                                            world_consumed_escape = vraylib.is_key_pressed(vraylib.KEY_ESCAPE) && (world_tools.state[1] != 0 || world_tools.state[5] != 0);
                                             try { if legacy || active_tab == 1 { se_world_tools_input(world_tools,ui,authored,history,camera,viewport); } }
                                            catch (error) { se_world_stroke_cancel(world_tools,authored.world); throw error; }
                                        }
                                         if ui.state[39] == 0 && ui.state[43] == 0 && !world_consumed_escape && !se_world_tools_busy(world_tools) && ui.state[35] == 0 && ui.state[13] == 0 && ui.state[1] == -1 && vraylib.is_key_pressed(vraylib.KEY_ESCAPE) {
                                             if ui.state[10] != 0 { ui.state[10] = 0; app.capture_mouse(false); }
                                             else { se_document_request(ui,authored,history,file_bytes,document,3,""); }
                                         }
                                         if ui.state[37] != 0 {
                                             ui.state[37] = 0;
                                              if se_host_dirty(host) && host.state[5] == 0 { host.state[0] = 1; host.state[5] = 2; }
                                              else { if se_host_flush_layout(host) { app.stop(); } else { host.state[0] = 1; host.state[5] = 2; } }
                                         }
                                         if ui.state[39] == 0 && ui.state[43] == 0 && ui.state[35] == 0 && ui.state[13] == 0 && !se_world_tools_busy(world_tools) {
                                             if legacy { se_splitters(ui,app.width(),app.height(),viewport); }
                                             else { se_host_splitters(host,ui,app.width(),app.height(),viewport); }
                                         }
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
                                                 if se_hover(viewport) && !se_view_controls_hit(ui,viewport) && (!legacy || mouse_position.y > viewport.y+114.0) && ui.state[1] == -1 && ui.state[13] == 0 && ui.state[16] == 0 && ui.state[17] == 0 && ui.state[18] == 0 && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) {
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
                                         if automated || shader_test { throw error; }
                                    }
                                    viewport = se_host_viewport(host,ui,app.width(),app.height(),legacy);
                                    if active_tab >= 2 && host.state[0] == 0 && ui.state[35] == 0 && ui.state[13] == 0 && !se_world_tools_busy(world_tools) {
                                        if host.state[13] != 0 { se_host_resolve_editable(host,bank); }
                                        host.workspace.state[8] = 0;
                                        if authored.valid(ui.state[0]) { if authored.kind(ui.state[0]) == 1 { ss_workspace_selected(host.workspace,se_node_model(authored,ui.state[0]%128,bank,cubes)); } }
                                        if active_tab == 3 { host.workspace.layout.state[1] = 3; }
                                        try { se_host_pipeline_prepare(app,host); if host.workspace.state[24] != 0 { se_host_pipeline_context(host); } success :: Bool := ss_workspace_frame(app,host.workspace,bounds,app.dt()); if shader_test && !success { throw "Hosted preview failed: "+se_text_read(host.workspace.log); } }
                                        catch (error) { se_message(logs,log_state,string(error)); if shader_test { throw error; } }
                                    }
                                    vw :: Int64 := int64(viewport.width); vh :: Int64 := int64(viewport.height);
                                    if target.width() != vw || target.height() != vh { target.resize(vw,vh); }
                                    if active_tab < 2 || legacy {
                                    if ui.state[4] == 0 { se_host_scene_resources(authored,bank); } else { se_host_scene_resources(runtime,bank); }
                                    target.begin();
                                    try {
                                        app.clear(vraylib.rgba(43,48,58,255));
                                        active_camera :: vraylib.Camera := camera; if ui.state[4] != 0 { active_camera = player.camera; }
                                        eye :: vraylib.Vector3 := active_camera.position(); bank.view[0] = eye.x; bank.view[1] = eye.y; bank.view[2] = eye.z;
                                        active_camera.begin();
                                        try {
                                            if legacy { vraylib.draw_grid(40,1.0); } else { se_host_grid(ui); }
                                            if ui.state[4] == 0 {
                                                render_world :: SEWorld := authored.world; if world_tools.state[5] != 0 { render_world = world_tools.staging; }
                                                se_draw_scene_resource(authored,author_cache,bank,cubes,true,ui.state[0],render_world);
                                                if world_tools.state[0] == 0 { se_gizmo_draw(ui,authored,author_cache); }
                                                se_world_tools_draw(world_tools,authored.world);
                                            }
                                            else { se_draw_scene(runtime,play_cache,bank,cubes,false,-1); }
                                        } finally { active_camera.end(); }
                                     } finally { target.end(); }
                                    }
                                    app.begin();
                                          try {
                                        app.clear(vraylib.rgba(19,23,29,255));
                                        target.texture().draw_region(se_rect(0.0,0.0,float64(vw),float64(vh)),viewport,vraylib.vec2(0.0,0.0),0.0,vraylib.WHITE);
                                        try {
                                             if active_tab < 2 || legacy {
                                             panels :: SEUI := ui;
                                             if ui.state[39] != 0 || ui.state[43] != 0 || ui.state[35] != 0 || ui.state[13] != 0 || ui.state[2] != -1 || se_world_tools_busy(world_tools) {
                                                 se_editor_freeze(ui,passive); panels = passive;
                                             }
                                             if legacy { se_toolbar(app,panels,authored,runtime,play_cache,player,history,file_bytes,app.width(),document); }
                                             else { se_host_toolbar(app,host,panels,authored,runtime,play_cache,player,history,file_bytes,document); }
                                             world_panel :: SEUI := ui;
                                             if ui.state[39] != 0 || ui.state[35] != 0 || ui.state[13] != 0 || ui.state[2] != -1 { se_editor_freeze(ui,passive); world_panel = passive; }
                                             if legacy { se_world_tools_panel(app,world_tools,world_panel,authored,history,viewport); }
                                             else { if active_tab == 1 && host.workspace.layout.state[4] == 0 { se_world_tools_panel(app,world_tools,world_panel,authored,history,se_rect(4.0,viewport.y+4.0,vmath.max(1.0,viewport.x-8.0),120.0)); } }
                                             # A toolbar/world button may have opened a dialog this frame.
                                             if ui.state[35] != 0 || ui.state[13] != 0 || se_world_tools_busy(world_tools) { se_editor_freeze(ui,passive); panels = passive; }
                                             se_view_controls(panels,camera,viewport);
                                             if legacy || host.workspace.layout.state[4] == 0 {
                                             tree_y :: Float64 := viewport.y-4.0; if !legacy && active_tab == 1 { tree_y = tree_y+124.0; }
                                             se_tree(app,panels,authored,history,work,se_rect(0.0,tree_y,viewport.x-4.0,vmath.max(1.0,viewport.y+viewport.height-tree_y+4.0)));
                                              se_inspector(app,panels,authored,history,schema,se_rect(viewport.x+viewport.width+4.0,viewport.y-4.0,float64(app.width())-viewport.x-viewport.width-4.0,viewport.height+8.0));
                                              if ui.state[44] != 0 && !se_world_tools_busy(world_tools) { ui.state[44] = 0; se_world_settings_begin(world_tools.settings,world_tools.numbers); se_editor_freeze(ui,passive); panels = passive; }
                                             panel_directory :: Array<Int64> := directory_bytes;
                                             if panels.state[13] == -1 { panel_directory = ui.asset_directory; }
                                             se_asset_panel(app,panels,authored,history,bank,panel_directory,file_bytes,se_rect(viewport.x,viewport.y+viewport.height+4.0,float64(app.width())-viewport.x,float64(app.height())-viewport.y-viewport.height-4.0));
                                             se_output(app,logs,log_state,se_rect(0.0,viewport.y+viewport.height+4.0,viewport.x-4.0,float64(app.height())-viewport.y-viewport.height-4.0));
                                             }
                                            if ui.state[6] != -1 { se_draw_text("Choose new parent in Scene tree",int64(viewport.x+12.0),int64(viewport.y+72.0),18,vraylib.YELLOW); }
                                            if ui.state[4] != 0 { se_draw_text("PLAY - click viewport to capture mouse; Escape releases",int64(viewport.x+12.0),int64(viewport.y+12.0),16,vraylib.YELLOW); }
                                              se_ui_overlays(panels);
                                             } else {
                                                  if host.state[0] == 0 && ui.state[35] == 0 && ui.state[13] == 0 && !se_world_tools_busy(world_tools) {
                                                      if active_tab == 3 { se_host_rendering_draw(app,host,bank,bounds); }
                                                      else { ss_workspace_draw(app,host.workspace,bounds); }
                                                      se_host_sampler_button(host,bounds,ui.state[4] == 0);
                                                  }
                                             }
                                             if ui.state[13] > 0 && ui.state[42] == 0 && !se_world_tools_modal_open(world_tools) {
                                                 if ui.state[13] == 4 || ui.state[13] == 5 { se_draw_text("Export resource map cells only (no terrain or node meshes)",20,app.height()-20,14,vraylib.YELLOW); }
                                                 if se_file_dialog(ui,se_rect(vmath.max(10.0,float64(app.width())*0.1),float64(app.height())*0.3,float64(app.width())*0.8,120.0)) { se_editor_commit(ui,authored,staging,history,bank,directory_bytes,file_bytes,document); }
                                             }
                                             if ui.state[42] == 0 { se_document_dialog(app,ui,authored,history,file_bytes,document); }
                                             se_world_tools_modal_draw(app,world_tools);
                                              se_editor_reload(ui,world_tools,authored.world);
                                              if !legacy {
                                                  previous :: Int64 := host.workspace.layout.state[0];
                                                  if host.state[0] == 0 && ui.state[35] == 0 && ui.state[13] == 0 && ui.state[1] == -1 && ui.state[2] == -1 && !se_world_tools_busy(world_tools) {
                                                      if se_host_navigation_ready(host,ui,active_tab) {
                                                      ss_workspace_navigation(host.workspace.layout,bounds);
                                                      if previous != host.workspace.layout.state[0] {
                                                          app.capture_mouse(false); ui.state[10] = 0; se_host_flush_layout(host);
                                                          if host.workspace.layout.state[0] == 3 { host.state[4] = host.workspace.layout.state[1]; host.workspace.layout.state[1] = 3; }
                                                          if previous == 3 && host.workspace.layout.state[0] == 2 { host.workspace.layout.state[1] = host.state[4]; }
                                                       }
                                                      }
                                                  }
                                                  se_host_dialog(app,host,ui,authored,history,file_bytes,document,world_tools);
                                              }
                                         } catch (error) { ui.state[21] = ui.state[21]+1; se_message(logs,log_state,string(error)); if automated || shader_test { throw error; } }
                                      } finally { app.end(); }
                                       se_host_actions(app,host,bank);
                                       se_host_pipeline_actions(app,host,bank);
                                       try { if ui.state[45] != 0 {
                                          action :: Int64 := ui.state[45];
                                          resource :: String := "";
                                          if action == 2 || action == 3 { resource = se_text_read(ui.external_path); }
                                         else { if authored.valid(ui.state[0]) { resource = se_text_read(authored.materials.paths[ui.state[0]%128]); if resource != "" && !vraylib.is_path_absolute(resource) { resource = vfs.join(project,resource); } } }
                                          ui.state[45] = 0;
                                          if action == 3 { se_host_bind(host,authored,history,bank,resource); }
                                          else { se_host_open(app,host,resource); if action == 1 && resource != "" { host.state[6] = ui.state[0]; } }
                                     }
                                      if ss_workspace_take_apply(host.workspace) {
                                          try { if ui.state[4] != 0 { throw "Stop Play before assigning an authored material"; } se_host_apply(app,host,authored,history,bank,ui.state[0]); }
                                          catch (error) { se_message(logs,log_state,string(error)); if shader_test { throw error; } }
                                      } } catch (error) { se_message(logs,log_state,string(error)); if shader_test { throw error; } }
                                    screenshot :: String := vmge_option("VYNE_VMGE_SCREENSHOT","");
                                     if automated && app.frames() == 14 && screenshot != "" { vraylib.take_screenshot(screenshot); }
                                     if shader_test {
                                         prefix :: String := vmge_option("VYNE_VMGE_SHADER_PREFIX","build/shader-host-dev-flow");
                                         if app.frames() == 8 { vraylib.take_screenshot(prefix+"-rendering.png"); }
                                         if app.frames() == 15 { vraylib.take_screenshot(prefix+"-scene.png"); }
                                         if app.frames() == 21 { vraylib.take_screenshot(prefix+"-shader.png"); }
                                     }
                                    if bank.state[1] != ui.state[25] { ui.state[25] = bank.state[1]; se_message(logs,log_state,"Missing asset: magenta bounds shown; edit its path to recover"); }
                                    } finally { app.transient_errors(false); }
                                } } finally { se_host_close(host,bank); }
                                frames = app.frames(); node_count = authored.count(); cycles = ui.state[20];
                                drift = vmem.total_allocated()-warm; commit_drift = vmem.commit_total_allocated()-commit_warm;
                                 out("VMGE_SCENE_EDITOR_STATE nodes=",node_count," play_cycles=",cycles," errors=",ui.state[21]," transient_headers=",drift," commit_drift=",commit_drift);
                                 if shader_test && (commit_drift != 0 || ui.state[21] != 0 || drift > 4096) { throw "Shader host allocation/error check failed"; }
                                 if shader_test { out("VMGE_SHADER_HOST_MEMORY_OK commit_drift=",commit_drift," source_owner_bytes=",drift); }
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
    if vraylib.live_handles() != 0 { throw "Scene host handles remain after shared App close"; }
    return frames;
}
fn se_shader_host_autotest(app :: vraylib.App,host :: SEWorkspaceHost,ui :: SEUI,scene :: SEScene,runtime :: SEScene,staging :: SEScene,history :: SEHistory,bank :: SEAssets,cubes :: Array,path :: Array<Int64>,cache :: SETransforms,player :: VMGEPlayer,document :: SEDocument) -> Bool {
    tick :: Int64 := app.frames()+1; w :: SSWorkspace := host.workspace;
    prefix :: String := vmge_option("VYNE_VMGE_SHADER_PREFIX","build/shader-host-dev-flow");
    if tick == 1 {
        se_text_write(w.document.path,prefix+".vshader"); se_text_write(host.editable_path,prefix+".vshader"); se_text_write(w.layout_path,prefix+".layout");
        se_text_write(host.graph_path,prefix+".vgraph"); se_text_write(host.pipeline_path,prefix+".vpipeline");
        se_demo(scene); scene.meta[13] = 0; ui.state[0] = scene.handle(2); se_history_reset(history,scene,true);
        w.layout.state[0] = 2; w.layout.state[1] = 0; w.state[14] = 1; w.state[35] = 1;
    }
    if tick == 2 {
        good :: Int64 := ss_program(w.document).token;
        se_text_write(w.fragment.edit_buffer,"bad shader"); ss_code_replace(w.fragment,0,w.document.source[0],w.fragment.edit_buffer); ss_document_source_change(w.document);
        se_studio_check(!ss_workspace_compile(app,w) && ss_program(w.document).token == good,"host failed candidate retains native program");
        se_host_reset(host); se_studio_check(ss_workspace_compile(app,w),"host compile recovery");
        out("VMGE_SHADER_HOST_COMPILE_OK");
    }
    if tick == 3 { ss_parameter_set(w.document,1,0,0.25); ss_workspace_parameter_override(w,1,0,0.25); ss_workspace_apply(w); }
    if tick == 4 { ui.state[0] = scene.handle(1); ss_parameter_set(w.document,1,0,0.75); ss_workspace_parameter_override(w,1,0,0.75); ss_workspace_apply(w); }
    if tick == 5 {
        se_studio_check(se_node_model(scene,1,bank,cubes).token == se_node_model(scene,2,bank,cubes).token,"two test meshes use the same cached model");
        low :: Float64 := se_host_probe(app,w,bank,scene,cubes,2); high :: Float64 := se_host_probe(app,w,bank,scene,cubes,1);
        se_studio_check(vmath.abs(low-0.25) < 0.01 && vmath.abs(high-0.75) < 0.01,"host per-draw materials produce independent float pixels");
        out("VMGE_SHADER_HOST_PIXELS_OK");
    }
    if tick == 6 {
        first :: MaterialInstance := scene.materials.instances[2]; second :: MaterialInstance := scene.materials.instances[1];
        se_studio_check(ss_material_get(first,1,0) == 0.25 && ss_material_get(second,1,0) == 0.75 && first.state[1] != second.state[1],"host independent node material parameters");
        se_undo(history,scene); se_studio_check(se_text_read(scene.materials.paths[1]) == "","assignment undo"); se_redo(history,scene);
        se_world_save_scene(scene,prefix+".vscene"); history.saved();
        se_world_load_scene(scene,staging,prefix+".vscene"); se_studio_check(ss_material_get(scene.materials.instances[2],1,0) == 0.25,"host reopen retains overrides");
        out("VMGE_SHADER_HOST_MATERIAL_OK");
        se_shader_host_immutable_review(app,host,scene,staging,history,bank,cubes,prefix);
    }
    if tick == 7 { w.layout.state[0] = 3; w.layout.state[1] = 3; w.state[24] = 1; w.state[12] = 1; ss_pipeline_enable(w.pipeline,1,true,true); }
    if tick == 8 {
        se_studio_check(w.state[26] == 1 && w.pipeline.outputs[0] != 0,"host actual pipeline capture");
        ss_pipeline_save(w.pipeline,prefix+".vpipeline"); ss_pipeline_load(w.pipeline,prefix+".vpipeline");
        out("VMGE_SHADER_HOST_PIPELINE_OK");
    }
    if tick == 9 { host.state[0] = 2; se_text_write(host.pending,se_text_read(w.document.path)); }
    if tick == 10 { se_studio_check(ui.state[39] == 1,"host modal isolates scene input"); host.state[0] = 0; host.pending[0] = 0; w.layout.state[0] = 0; w.state[39] = 0; w.state[44] = 5126; w.state[45] = 1; se_edit_begin(w.parameter_editor,0,1,"0.5"); }
    if tick == 11 {
        se_studio_check(host.state[14] == 1 && !se_host_navigation_ready(host,ui,0),"exact-entry field zero owns navigation and scene input");
        host.state[0] = 1; se_host_choice(app,host,ui,scene,history,path,document,0);
        se_studio_check(w.parameter_editor.state[1] == 0 && se_host_dirty(host),"Close Cancel retains pending exact-entry text");
        se_studio_check(se_host_save_editable(host) && w.parameter_editor.state[1] == -1 && ss_material_get(w.material,1,0) == 0.5,"Save validates and commits exact-entry field zero");
        se_studio_check(!se_host_navigation_ready(host,ui,0),"committed field still owns its captured frame");
        se_toggle_play(ui,scene,runtime,cache,player,app); out("VMGE_SHADER_HOST_INPUT_OK");
    }
    if tick == 12 { se_studio_check(ss_material_get(runtime.materials.instances[2],1,0) == 0.25 && se_text_read(runtime.materials.paths[2]) != "","host Play retains material resources"); }
    if tick == 13 { se_toggle_play(ui,scene,runtime,cache,player,app); }
    if tick == 14 { vraylib.set_window_size(900,600); w.layout.state[0] = 1; }
    if tick == 15 {
        ss_parameter_define(w.document,2,"surfaceTexture",35678,1,0.0,0.0,0.0,"Surface","resource",false,false);
        se_text_write(w.document.source,"#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nuniform float gain;\nuniform sampler2D surfaceTexture;\nvoid main()\{finalColor=texture(surfaceTexture,fragTexCoord)*vec4(vec3(gain),1.0);}\n"); ss_document_source_change(w.document);
        se_studio_check(ss_workspace_compile(app,w),"host sampler compile and reflection");
        se_studio_check(se_host_save_editable(host),"save sampler schema before binding");
        source :: String := vfs.join(vraylib.get_working_directory(),prefix+"-texture-a.png");
        se_host_bind(host,scene,history,bank,source);
        se_studio_check(!ss_document_dirty(w.document) && se_host_material_dirty(host) && se_host_dirty(host),"sampler-only edits are dirty");
        se_studio_check(se_host_save_editable(host),"atomically save bound editable material");
        ui.state[0] = scene.handle(2); ss_workspace_apply(w);
    }
    if tick == 16 {
        se_text_write(host.pending,se_host_material_path(se_text_read(host.editable_path))); host.state[0] = 2; se_host_finish(app,host);
        se_studio_check(w.material.binding_state[0] == 1 && w.material.bindings[2] == 0 && w.material.bindings[3] == 0,"editable checkpoint excludes native tokens");
        se_host_resolve_editable(host,bank);
        se_studio_check(w.material.bindings[2] != 0 && host.resource_state[0] == 1 && !se_host_material_dirty(host),"editable reopen resolves saved R paths");
        se_host_bind(host,scene,history,bank,vfs.join(vraylib.get_working_directory(),prefix+"-texture-b.png"));
        host.state[0] = 1; se_host_choice(app,host,ui,scene,history,path,document,0);
        se_studio_check(se_host_material_dirty(host),"sampler-only close Cancel retains bindings");
        se_host_reset(host); se_studio_check(w.material.bindings[1] == 100 && host.resource_state[0] == 1 && !se_host_material_dirty(host),"Reset restores saved material and R table");
        se_studio_check(ss_workspace_compile(app,w),"saved sampler Reset compiles");
        w.layout.state[0] = 2; w.state[15] = 128; out("VMGE_SHADER_HOST_EDITABLE_MATERIAL_OK");
    }
    if tick == 18 { se_studio_check(w.state[26] == 0,"host resize invalidates capture"); }
    if tick >= 19 && tick <= 22 {
        ss_parameter_set(w.document,1,0,float64(tick-18)*0.1); ss_workspace_parameter_override(w,1,0,float64(tick-18)*0.1); ss_workspace_apply(w);
    }
    if tick == 24 {
        se_studio_check(bank.programs.state[0] == 8,"all eight preframe published source copies compiled");
        value :: Float64 := ss_material_get(scene.materials.instances[2],1,0); ss_parameter_set(w.document,1,0,0.9); ss_workspace_parameter_override(w,1,0,0.9);
        rejected :: Bool := false; try { se_host_apply(app,host,scene,history,bank,ui.state[0]); } catch (error) { rejected = true; }
        se_studio_check(rejected && ss_material_get(scene.materials.instances[2],1,0) == value && bank.programs.state[0] == 8,"bounded registry rejects ninth publication atomically");
        out("VMGE_SHADER_HOST_REGISTRY_OK");
    }
    if tick == 25 {
        se_world_save_scene(scene,prefix+"-textures.vscene"); se_world_load_scene(scene,staging,prefix+"-textures.vscene");
        m :: MaterialInstance := scene.materials.instances[2]; se_studio_check(m.bindings[2] == 0 && m.bindings[3] == 0 && m.bindings[1] >= 100,"texture reopen contains only resource references");
        se_material_resolve_textures(scene.materials,m,bank); se_studio_check(m.bindings[2] != 0 && m.bindings[3] == app.token,"texture references resolve in shared App");
        out("VMGE_SHADER_HOST_RESOURCES_OK");
    }
    if tick == 26 { se_toggle_play(ui,scene,runtime,cache,player,app); }
    if tick == 27 { se_studio_check(se_text_read(runtime.materials.paths[2]) != "" && runtime.materials.resource_state[0] == 1,"Play keeps sampler resource table"); }
    if tick == 28 { se_toggle_play(ui,scene,runtime,cache,player,app); }
    if tick == 29 {
        hash :: Int64 := ss_hash(w.document.source); se_host_open(app,host,prefix+"-missing.vshader");
        se_studio_check(host.state[0] == 2,"dirty source open requires confirmation");
        se_host_choice(app,host,ui,scene,history,path,document,0);
        se_studio_check(host.state[0] == 0 && ss_hash(w.document.source) == hash,"cancel source replacement preserves source");
    }
    if tick == 30 {
        se_text_write(w.layout_path,prefix+"-missing/layout"); host.state[0] = 1;
        se_host_choice(app,host,ui,scene,history,path,document,1);
        se_studio_check(host.state[0] == 1 && host.state[10] == 1 && se_host_dirty(host),"layout failure retains pending close and dirty state");
        se_text_write(w.layout_path,prefix+".layout");
        out("VMGE_SHADER_HOST_LAYOUT_FAILURE_OK");
        se_text_write(w.document.path,prefix+"-missing/failed.vshader"); host.state[0] = 1;
        se_host_choice(app,host,ui,scene,history,path,document,1);
        se_studio_check(host.state[0] == 1 && ss_document_dirty(w.document),"failed studio save retains dirty confirmation");
        se_text_write(w.document.path,prefix+".vshader");
        se_host_choice(app,host,ui,scene,history,path,document,2);
        se_studio_check(ui.state[35] == 1 && ui.state[34] == 3,"studio Discard still confirms unsaved scene");
        se_document_choice(ui,scene,history,path,document,0); host.state[5] = 0;
        se_studio_check(scene.count() == 5 && ui.state[37] == 0 && ss_document_dirty(w.document),"scene Cancel preserves scene and dirty studio");
        out("VMGE_SHADER_HOST_CLOSE_OK");
    }
    if tick == 31 { out("VMGE_SHADER_HOST_AUTOTEST_OK"); }
    return true;
}
fn se_host_autotest_prepare(app :: vraylib.App,host :: SEWorkspaceHost,prefix :: String) -> Bool {
    image :: vraylib.Image := app.image(16,16,vraylib.WHITE);
    try { se_studio_check(image.save(prefix+"-texture-a.png"),"save procedural sampler fixture A"); } finally { image.close(); }
    other :: vraylib.Image := app.image(16,16,vraylib.rgba(64,160,224,255));
    try { se_studio_check(other.save(prefix+"-texture-b.png"),"save procedural sampler fixture B"); } finally { other.close(); }
    w :: SSWorkspace := host.workspace; w.document.parameters.state[0] = 0;
    ss_parameter_define(w.document,1,"gain",5126,1,0.5,0.0,1.0,"Surface","linear",true,true);
    se_text_write(w.document.source,"#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nuniform float gain;\nvoid main()\{finalColor=vec4(vec3(gain),1.0);}\n");
    w.document.vertex[0] = 0; w.document.fragment_path[0] = 0; w.document.vertex_path[0] = 0;
    se_text_write(w.document.path,prefix+".vshader"); se_text_write(host.editable_path,prefix+".vshader");
    se_text_write(w.layout_path,prefix+".layout"); se_text_write(host.graph_path,prefix+".vgraph"); se_text_write(host.pipeline_path,prefix+".vpipeline");
    se_material_reset(w.material,1,1); host.resource_state[0] = 0; w.panel.draft[0] = 0; w.panel.state[2] = 0; w.parameter_editor.state[1] = -1;
    ss_document_source_change(w.document); se_studio_check(ss_workspace_compile(app,w),"deterministic hosted fixture compile");
    se_text_copy(w.saved_source,w.document.source); se_text_copy(w.saved_vertex,w.document.vertex); se_host_checkpoint(host);
    w.state[55] = 0; w.panel.state[14] = 0; return true;
}
fn se_shader_host_authoring_autotest(app :: vraylib.App,host :: SEWorkspaceHost,ui :: SEUI,scene :: SEScene,runtime :: SEScene,history :: SEHistory,bank :: SEAssets,cubes :: Array,path :: Array<Int64>,cache :: SETransforms,player :: VMGEPlayer,document :: SEDocument) -> Bool {
    tick :: Int64 := app.frames()+1; w :: SSWorkspace := host.workspace; prefix :: String := vmge_option("VYNE_VMGE_SHADER_PREFIX","build/shader-host-dev-authoring");
    if tick == 1 {
        se_demo(scene); se_history_reset(history,scene,true); ui.state[0] = scene.handle(2); w.layout.state[0] = 2; w.layout.state[1] = 0; w.state[14] = 1; w.state[35] = 1;
        se_studio_check(w.documents.size() == 10 && w.document_ids[0] == 3 && w.document_ids[8] == 1 && w.document_ids[9] == 2,"ten preframe shared documents"); ss_workspace_save(w);
    }
    if tick == 2 {
        se_studio_check(ss_workspace_schema_begin(w,1),"open schema draft"); se_edit_begin(w.parameter_editor,102,1,"Authoring");
        se_host_commit_parameter(host); se_studio_check(se_text_read(w.panel.group_name) == "Authoring" && ss_workspace_input_captured(w),"host dispatches metadata field IDs");
        se_studio_check(ss_workspace_schema_apply(app,w),"apply metadata schema"); ss_workspace_save(w);
    }
    if tick == 3 {
        se_studio_check(se_text_read(w.document.parameters.groups[0]) == "Authoring" && !ss_document_dirty(w.document),"metadata Save edge committed");
        se_text_write(w.document.source,"#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nuniform float gain;\nuniform vec3 tint;\nvoid main()\{finalColor=vec4(vec3(gain)*tint,1.0);}\n"); ss_document_source_change(w.document);
        se_studio_check(ss_workspace_compile(app,w) && w.panel.state[0] == 1,"real tint reflection"); se_studio_check(ss_workspace_expose_begin(w,0),"expose vec3 uniform");
        se_edit_begin(w.parameter_editor,200,w.panel.draft[1],"0.8"); se_host_commit_parameter(host);
        se_studio_check(ss_workspace_schema_apply(app,w) && w.document.parameters.state[0] == 2,"exposed defaults and metadata dispatch"); ss_workspace_save(w);
    }
    if tick == 4 {
        se_studio_check(ss_workspace_save_as_begin(w),"Save As begins"); se_text_write(w.parameter_editor.text,prefix+"-renamed.vshader");
        ss_workspace_commit_field(w); if w.panel.state[2] == 2 { ss_workspace_save_as_request(w,prefix+"-renamed.vshader",true); }
        se_studio_check(w.state[47] == 1 && w.panel.state[2] == 3,"path field raises hosted Save As");
    }
    if tick == 5 {
        expected :: String := vfs.join(vraylib.get_working_directory(),prefix+"-renamed.vshader");
        se_studio_check(ss_normal_path(se_text_read(w.document.path)) == ss_normal_path(expected) && w.panel.state[2] == 0 && !ss_document_dirty(w.document),"Save As identity and material acknowledged");
        ss_workspace_parameter_set(w,1,0,0.75); ss_workspace_save_as_begin(w); se_text_write(w.parameter_editor.text,prefix+"-missing/failure.vshader"); ss_workspace_commit_field(w);
    }
    if tick == 6 {
        se_studio_check(vfs.basename(se_text_read(w.document.path)) == vfs.basename(prefix+"-renamed.vshader") && w.panel.state[2] == 1 && w.parameter_editor.state[1] == 900 && ss_material_get(host.saved_material,1,0) == 0.5,"failed Save As preserves identity, checkpoint and path focus");
        w.panel.state[2] = 0; w.parameter_editor.state[1] = -1; w.state[55] = 1;
        se_studio_check(!ss_workspace_reload(w,false) && w.panel.state[2] == 4,"Reload asks before discarding local values"); ss_workspace_reload(w,true);
    }
    if tick == 7 {
        se_studio_check(w.panel.state[2] == 0 && host.state[0] == 0 && ss_material_get(w.material,1,0) == 0.5 && w.fragment.state[7] == 0,"confirmed Reload restores checkpoint without a second host modal");
        ss_workspace_schema_begin(w,1); se_studio_check(w.parameter_editor.state[1] == -1 && ss_workspace_input_captured(w) && !se_host_navigation_ready(host,ui,2),"unfocused schema draft still captures navigation");
        ss_workspace_reset(w);
    }
    if tick == 8 {
        se_studio_check(w.state[46] == 0 && w.panel.draft[0] == 0,"Reset acknowledgment clears metadata draft and histories");
        host.render_state[0] = 2; se_studio_check(se_host_pipeline_actions(app,host,bank),"load bloom through the deferred action dispatcher"); w.layout.state[0] = 3;
        out("VMGE_SHADER_HOST_AUTHORING_OK"); out("VMGE_SHADER_HOST_SAVE_AS_RELOAD_OK");
    }
    if tick == 9 {
        se_studio_check(bank.programs.state[0] == 5 && w.pipeline.state[0] == 5 && w.pipeline.meta[1] == 4 && w.pipeline.meta[41] == 3,"bloom gallery remaps 201..205 into shared bank slots: "+se_text_read(w.log));
        input :: vraylib.RenderTarget := vraylib.RenderTarget(w.state[1],app.token); output :: vraylib.RenderTarget := vraylib.RenderTarget(w.state[25],app.token);
        a :: Array<Float64> := input.read_pixels(); b :: Array<Float64> := output.read_pixels(); center :: Int64 := (256*512+256)*4;
        se_studio_check(output.width() == 512 && vmath.abs(a[center]-b[center]) > 0.05 && b[center+3] > 0.99,"gallery runs real linear five-pass pixels");
        before :: String := ss_pipeline_text(w.pipeline); se_studio_check(!se_host_pipeline_edit(host,2,10,50) && before == ss_pipeline_text(w.pipeline),"invalid forward input rolls back");
        se_studio_check(se_host_pipeline_edit(host,1,50,2) && se_host_pipeline_edit(host,5,50,0) && se_host_pipeline_edit(host,11,50,128),"shader, format and extent edits commit");
    }
    if tick == 10 {
        output :: vraylib.RenderTarget := vraylib.RenderTarget(w.state[25],app.token); pixels :: Array<Float64> := output.read_pixels();
        se_studio_check(output.width() == 128 && vmath.abs(pixels[(64*128+64)*4]-0.4) < 0.02,"custom identity pass and RGBA8 extent change actual pixels");
        se_studio_check(se_host_pipeline_edit(host,3,40,-1) && se_host_pipeline_edit(host,4,10,2),"second input and divisor edits");
        before :: String := ss_pipeline_text(w.pipeline); se_studio_check(!se_host_pipeline_edit(host,7,50,0) && !se_host_pipeline_edit(host,6,10,0) && before == ss_pipeline_text(w.pipeline),"dependent reorder/remove roll back");
        se_studio_check(se_host_pipeline_edit(host,0,60,2) && se_host_pipeline_edit(host,2,60,-1) && se_host_pipeline_edit(host,7,60,0) && ss_pipeline_slot(w.pipeline,60) == 4 && se_host_pipeline_edit(host,6,60,0),"independent add, input, reorder and remove workflows");
    }
    if tick == 11 {
        output :: vraylib.RenderTarget := vraylib.RenderTarget(w.state[25],app.token); pixels :: Array<Float64> := output.read_pixels(); se_studio_check(pixels[(64*128+64)*4] > 0.5,"composite second input affects output");
        se_studio_check(se_host_pipeline_save(host,bank),"save pass paths and descriptors");
    }
    if tick == 12 { se_studio_check(se_host_pipeline_load(app,host,bank,se_text_read(host.pipeline_path)) && bank.programs.state[0] == 5,"reopen pass ledger reuses the fixed bank"); out("VMGE_SHADER_HOST_RENDERING_GALLERY_OK"); }
    if tick == 13 {
        se_host_open(app,host,vfs.join(bank.root,"shaders/studio/lit-material.vshader")); if host.state[0] == 2 { se_host_choice(app,host,ui,scene,history,path,document,2); }
        ss_workspace_parameter_set(w,12,0,1.0); ui.state[0] = scene.handle(2); ss_workspace_apply(w);
    }
    if tick == 14 {
        se_studio_check(scene.materials.instances[2].state[2] == 12 && bank.programs.state[0] == 6,"gallery lit material applies through the ordinary inspector workflow");
        pixel :: Float64 := se_host_probe(app,w,bank,scene,cubes,2); se_studio_check(pixel >= 0.49 && pixel <= 1.01,"normal-bearing model uses the real lit vertex/fragment program"); out("VMGE_SHADER_HOST_LIT_APPLY_OK");
        se_program_resolve(bank.programs,app,vfs.join(bank.root,"shaders/studio/lit-material.vshader")); se_program_resolve(bank.programs,app,vfs.join(bank.root,"shaders/studio/plasma-grid.vshader"));
    }
    if tick == 15 {
        before :: String := ss_pipeline_text(w.pipeline); se_studio_check(bank.programs.state[0] == 8,"all shared source slots are compiled");
        @bounded<16777216> region authoring_capacity_fixture {
            d :: SSDocument := bank.programs.documents[0]; se_text_copy(host.publication.source,d.source); se_text_copy(host.publication.vertex,d.vertex); ss_parameters_copy(host.publication.parameters,d.parameters);
            ss_document_save(host.publication,prefix+"-extra.vshader"); bytes :: Array<Int64> := ss_text(2048); se_text_write(bytes,vfs.join(vraylib.get_working_directory(),prefix+"-extra.vshader"));
            vfs.write_atomic(prefix+"-overflow.vpipeline","VMGE_RENDER_PIPELINE|1\nVMGE_PIPELINE|1\nP|1|3|-1|-2|1|0|0|1|1|0\nD|3|"+se_text_hex(bytes)+"\n");
        };
        se_studio_check(!se_host_pipeline_load(app,host,bank,vfs.join(vraylib.get_working_directory(),prefix+"-overflow.vpipeline")) && bank.programs.state[0] == 8 && before == ss_pipeline_text(w.pipeline),"gallery capacity preflight preserves history and pipeline"); out("VMGE_SHADER_HOST_RENDER_BANK_OK");
    }
    if tick == 17 { w.layout.state[0] = 0; se_toggle_play(ui,scene,runtime,cache,player,app); }
    if tick == 18 { se_studio_check(runtime.materials.instances[2].state[2] == 12,"Play retains gallery material"); }
    if tick == 19 { se_toggle_play(ui,scene,runtime,cache,player,app); }
    if tick == 20 { vraylib.set_window_size(900,600); w.layout.state[0] = 3; }
    if tick == 31 { se_studio_check(app.passes() == 0,"authoring pipeline passes balance"); out("VMGE_SHADER_HOST_AUTHORING_AUTOTEST_OK"); }
    return true;
}
fn se_shader_host_immutable_review(app :: vraylib.App,host :: SEWorkspaceHost,scene :: SEScene,staging :: SEScene,history :: SEHistory,bank :: SEAssets,cubes :: Array,prefix :: String) -> Bool {
    w :: SSWorkspace := host.workspace;
    @bounded<67108864> region host_publication_regression {
        original :: String := se_text_read(scene.materials.paths[2]); if !vraylib.is_path_absolute(original) { original = vfs.join(bank.root,original); }
        before := vfs.read(original); se_text_write(host.pending,original); host.state[0] = 2; se_host_finish(app,host);
        se_host_selected_material(host,scene,bank,scene.handle(2));
        se_studio_check(se_text_read(w.document.path) == se_text_read(host.editable_path) && se_text_read(w.document.path) != original,"publication opens into a separate editable document");
        ss_parameter_set(w.document,1,0,0.6); ss_workspace_parameter_override(w,1,0,0.6);
        se_text_write(w.document.source,se_text_read(w.document.source)+"\n// editable studio copy\n"); ss_document_source_change(w.document);
        se_studio_check(ss_workspace_compile(app,w),"editable publication copy compiles");
        se_text_write(w.document.path,original);
        se_studio_check(!se_host_save_editable(host),"host save boundary rejects immutable publication edits");
        se_text_copy(w.document.path,host.editable_path); se_studio_check(se_host_save_editable(host),"editable publication copy saves");
        after := vfs.read(original); se_studio_check(after != null && after == before,"Save preserves the original publication bytes");
        se_world_load_scene(scene,staging,prefix+".vscene"); ss_document_load(host.publication,original);
        se_studio_check(ss_parameter_get(host.publication,1,0) == 0.25 && ss_material_get(scene.materials.instances[2],1,0) == 0.25,"reopened original scene and publication retain material A");
        out("VMGE_SHADER_HOST_IMMUTABLE_OK");
        include_path :: String := prefix+"-include.glsl";
        se_studio_check(vfs.write_atomic(include_path,"vec3 frozen_gain(float g)\{return vec3(g);}\n"),"create include fixture");
        se_text_write(w.document.source,"#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nuniform float gain;\n#include \""+vfs.basename(include_path)+"\"\nvoid main()\{finalColor=vec4(frozen_gain(gain),1.0);}\n");
        se_text_write(w.document.fragment_path,prefix+".vshader"); ss_document_source_change(w.document);
        se_studio_check(ss_workspace_compile(app,w),"compile include-backed source");
        se_host_apply(app,host,scene,history,bank,scene.handle(2));
        frozen :: String := se_text_read(scene.materials.paths[2]); if !vraylib.is_path_absolute(frozen) { frozen = vfs.join(bank.root,frozen); }
        se_studio_check(vfs.write_atomic(include_path,"vec3 frozen_gain(float g)\{return vec3(g)*0.0;}\n"),"mutate authored include after Apply");
        ss_document_load(host.publication,frozen);
        se_studio_check(!se_contains(se_text_read(host.publication.source),"#include") && ss_workspace_status(w) == "Compiled","published include content is self-contained");
        se_studio_check(ss_compile(app,host.publication),"reopened frozen include publication compiles independently");
        pixel :: Float64 := se_host_document_pixel(app,w,host.publication,w.material,se_node_model(scene,2,bank,cubes));
        se_studio_check(vmath.abs(pixel-0.6) < 0.01,"include mutation cannot change reopened publication pixels");
        se_world_load_scene(scene,staging,prefix+".vscene"); out("VMGE_SHADER_HOST_INCLUDE_OK");
        b :: SSDocument := host.publication; b.parameters.state[0] = 0;
        ss_parameter_define(b,1,"gain",5126,1,0.42,0.0,1.0,"Surface","linear",true,true);
        se_text_copy(b.source,w.saved_source); se_text_write(b.fragment_path,prefix+"-B.vshader"); se_text_write(b.vertex_path,prefix+"-B.vshader");
        ss_document_save(b,prefix+"-B.vshader"); se_text_write(host.pending,prefix+"-B.vshader"); host.state[0] = 2; se_host_finish(app,host);
        se_studio_check(ss_material_get(w.material,1,0) == 0.42 && w.material.binding_state[0] == 0 && w.material.state[2] == 1,"document B seeds its own defaults despite matching stable IDs");
        ss_parameter_set(w.document,1,0,0.81); ss_workspace_parameter_override(w,1,0,0.81); se_host_reset(host);
        se_studio_check(ss_material_get(w.material,1,0) == 0.42 && ss_parameter_get(w.document,1,0) == 0.42 && ss_document_dirty(w.document),"Reset restores loaded B while retaining its unsaved editable-copy marker");
        se_text_copy(host.pending,host.editable_path); host.state[0] = 2; se_host_finish(app,host);
        se_host_resolve_editable(host,bank);
        se_studio_check(ss_material_get(w.material,1,0) == 0.6 && !ss_document_dirty(w.document),"atomic editable checkpoint restores the saved copy");
        out("VMGE_SHADER_HOST_RESEED_OK");
    }; return true;
}
