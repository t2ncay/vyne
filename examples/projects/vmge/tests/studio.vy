use "../src/editor/scene_editor.vy";

# CPU document/IO and real managed-frame input-dispatch checks. All fixture paths
# are outside the project; the prefix parent must already exist.
before :: Int64 := vmem.total_allocated();
@slab<Int64,256,512> region studio_owner {
    names :: Array := []; paths :: Array := []; assets :: Array := []; i :: Int64 := 0;
    while i < 384 { n :: Array<Int64> := slab_alloc(64); p :: Array<Int64> := slab_alloc(256); names.push(n); paths.push(p); i = i+1; }
    i = 0; while i < 64 { p :: Array<Int64> := slab_alloc(256); assets.push(p); i = i+1; }
    edit :: Array<Int64> := slab_alloc(256); path :: Array<Int64> := slab_alloc(256); directory :: Array<Int64> := slab_alloc(256);
    authored :: SEScene := se_scene(names,paths,0); runtime :: SEScene := se_scene(names,paths,128); staging :: SEScene := se_scene(names,paths,256);
    ui :: SEUI := se_ui(edit); passive :: SEUI := se_editor_passive(ui);
    tools :: SEWorldTools := se_world_tools(); document :: SEDocument := se_document();
    prefix :: String := vmge_option("VYNE_VMGE_STUDIO_PREFIX","build/studio-dev-test");
    fixture :: String := prefix+".vscene"; se_text_write(path,fixture); se_text_write(directory,vfs.dirname(fixture));
    app :: vraylib.App := vraylib.session(""); app.limit(16);
    bank :: SEAssets := se_assets(app,assets,vmge_project_root());
    player :: VMGEPlayer := vmge_player(app);
    matrices :: Array<Float64> := []; matrices.reserve(2048); matrices.resize(2048,0.0); cache :: SETransforms := se_transforms(matrices);
    try {
    @ring<Float64,17,2048> region studio_numeric_history {
        numbers :: Array := []; i = 0; while i < 17 { slot :: Array<Float64> := ring_alloc(2048); numbers.push(slot); i = i+1; }
        @ring<Int64,17,41988> region studio_text_history {
            texts :: Array := []; i = 0; while i < 17 { slot :: Array<Int64> := ring_alloc(41988); texts.push(slot); i = i+1; }
            history :: SEHistory := se_history(numbers,texts);
            se_demo(authored); se_terrain_generate_procedural(authored.world,4,2.0,1.5,77,0,0.125,3,0.625,3.0);
            se_world_paint_data(authored.world.data,20.0,0.5,20.0,1.0,2); se_history_reset(history,authored,false);
            se_edit_begin(ui,16,-1,fixture); ui.state[13] = 2;
            se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
            se_studio_check(!history.dirty() && ui.state[36] == 1,"save publishes document identity only after success");
            snapshot :: String := se_world_scene_text(authored);
            se_clear(authored); se_history_reset(history,authored,true);
            se_edit_begin(ui,16,-1,fixture); ui.state[13] = 1; se_editor_commit(ui,authored,staging,history,bank,directory,path,document); se_editor_reload(ui,tools,authored.world);
            se_studio_check(se_world_scene_text(authored) == snapshot && tools.numbers[5] == 77.0 && tools.numbers[21] == 0.125 && tools.numbers[23] == 0.625,"commit roundtrips exact G metadata and reloads options");
            authored.property(authored.handle(2),0,9.0); se_record(history,authored); snapshot = se_world_scene_text(authored);
            # Open an asset path while dirty: defer IO and retain it through Save As.
            se_edit_begin(ui,16,-1,fixture); ui.state[13] = 1;
            se_studio_check(!se_editor_commit(ui,authored,staging,history,bank,directory,path,document) && ui.state[35] == 1 && ui.state[34] == 2,"dirty field16 Open is intercepted before load");
            se_studio_check(se_world_scene_text(authored) == snapshot,"pending Open does not mutate");
            se_document_choice(ui,authored,history,path,document,0);
            se_studio_check(ui.state[34] == 0 && se_world_scene_text(authored) == snapshot && history.dirty(),"Cancel pending Open preserves scene and dirty revision");
            se_document_request(ui,authored,history,path,document,3,"");
            se_document_choice(ui,authored,history,path,document,0);
            se_studio_check(ui.state[37] == 0 && history.dirty(),"Cancel Exit does not quit or clean");
            se_document_request(ui,authored,history,path,document,1,""); se_document_choice(ui,authored,history,path,document,0);
            se_studio_check(se_world_scene_text(authored) == snapshot,"Cancel New preserves all resource and node data");
            # Failed direct Save while confirming Open retains pending path/action.
            se_text_write(path,prefix+"-absent/failed.vscene");
            se_document_request(ui,authored,history,path,document,2,fixture);
            failed :: Bool := false; try { se_document_choice(ui,authored,history,path,document,1); } catch (error) { failed = true; }
            se_studio_check(failed && ui.state[34] == 2 && ui.state[35] == 1 && se_text_read(document.pending_path) == fixture && history.dirty() && se_world_scene_text(authored) == snapshot,"failed direct save preserves dirty pending document");
            # Failed Save As remains retryable, then saves and resumes the original Open.
            se_document_save(ui,authored,history,path,document,true);
            failed = false; try { se_editor_commit(ui,authored,staging,history,bank,directory,path,document); } catch (error) { failed = true; }
            se_studio_check(failed && ui.state[13] == 2 && ui.state[34] == 2 && history.dirty(),"failed Save As retains focused path and pending action");
            se_edit_begin(ui,16,-1,prefix+"-saved-as.vscene"); ui.state[13] = 2;
            se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
            se_studio_check(!history.dirty() && ui.state[13] == 1 && se_text_read(ui.text) == fixture && se_text_read(path) == prefix+"-saved-as.vscene","successful Save As resumes original Open path");
            se_editor_commit(ui,authored,staging,history,bank,directory,path,document); se_editor_reload(ui,tools,authored.world);
            se_studio_check(authored.number(authored.handle(2),0) != 9.0 && se_text_read(path) == fixture,"pending Open executes only after successful save");
            # Failed load must also leave authored payload, path, and history intact.
            snapshot = se_world_scene_text(authored);
            se_edit_begin(ui,16,-1,prefix+"-absent/open.vscene"); ui.state[13] = 1;
            failed = false; try { se_editor_commit(ui,authored,staging,history,bank,directory,path,document); } catch (error) { failed = true; }
            se_studio_check(failed && !history.dirty() && se_text_read(path) == fixture && se_world_scene_text(authored) == snapshot,"failed load is atomic through editor commit"); se_document_cancel(ui,document);
            se_edit_begin(ui,16,-1,prefix+".obj"); ui.state[13] = 4; se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
            se_edit_begin(ui,16,-1,prefix+".dat"); ui.state[13] = 5; se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
            imported :: VMGEMap := vmge_map_load(prefix+".dat",4096,false);
            se_studio_check(imported.count() == 1 && se_text_read(path) == fixture && !history.dirty(),"exports include resource cells only and preserve document path");
            se_record(history,authored); se_document_request(ui,authored,history,path,document,3,"");
            se_document_choice(ui,authored,history,path,document,1);
            se_studio_check(ui.state[37] == 1 && ui.state[13] == 0 && !history.dirty(),"direct Save resumes pending Exit only after success"); ui.state[37] = 0;
            se_copy(staging,authored); staging.property(staging.handle(4),0,6.0);
            se_play_start(staging,runtime,cache,player);
            if vmge_setting("VYNE_VMGE_SPAWN_PAD",0) == 1 { se_studio_check(player.camera.position().x == runtime.world.data[14] && player.camera.position().z == runtime.world.data[15] && player.state[1] == 0.0,"explicit spawn pad option"); }
            else { se_studio_check(player.camera.position().x == 6.0,"authored camera preserved by default"); }
            # Actual Play horizontal probes: a grounded walkable 0.2 rise, followed
            # by a steep triangle, crouch transition and ordinary node headroom.
            se_clear(runtime); character :: Int64 := runtime.spawn(4,"Walker",-1);
            se_terrain_generate(runtime.world,1,1.0,0.0,42,3);
            runtime.world.data[20497] = 0.8; runtime.world.data[20562] = 0.8;
            se_update_transforms(runtime,cache); player.reset(); player.camera.set_view(vraylib.vec3(0.0,1.8,0.1),vraylib.vec3(1.0,1.8,0.1)); player.state[2] = 1.0;
            se_character_update(runtime,cache,player,character,1.0,0.0,false,false,false,0.025);
            se_studio_check(player.camera.position().x > 0.15 && player.camera.position().y > 1.92,"integrated Play follows slope above legacy .15 proxy");
            runtime.world.data[20497] = 2.0; runtime.world.data[20562] = 2.0;
            player.camera.set_view(vraylib.vec3(0.0,1.8,0.1),vraylib.vec3(1.0,1.8,0.1)); player.state[2] = 1.0;
            se_character_update(runtime,cache,player,character,1.0,0.0,false,false,false,0.025);
            se_studio_check(player.camera.position().x == 0.0,"integrated Play blocks steep uphill");
            se_character_update(runtime,cache,player,character,0.0,0.0,false,true,false,0.0);
            se_studio_check(player.height() == 0.9,"crouch preserved");
            roof :: Int64 := runtime.spawn(3,"Roof",-1); runtime.property(roof,0,0.0); runtime.property(roof,1,1.65); runtime.property(roof,2,0.1); se_update_transforms(runtime,cache);
            se_character_update(runtime,cache,player,character,0.0,0.0,false,false,false,0.0);
            se_studio_check(player.height() == 0.9,"standing blocked by ordinary headroom");
            # Real dispatch over managed frames, with simultaneous inspector focus.
            se_edit_begin(ui,14,authored.handle(2),"Inspector focus");
            warm :: Int64 := vmem.commit_total_allocated();
            while app.frame() {
                se_world_settings_begin(tools.settings,tools.numbers); se_edit_begin(tools.settings.editor,0,-1,"123");
                se_studio_check(!se_editor_input(ui,tools,document) && ui.state[39] == 1 && ui.state[42] == 1 && ui.state[40] == app.frames()+1,"one modal-owned input dispatch per frame");
                se_editor_cancel_focus(ui,tools,document);
                se_studio_check(ui.state[39] == 1 && se_text_read(ui.text) == "Inspector focus" && se_world_scene_text(authored) == snapshot,"closing modal cannot release frame or double-input inspector");
                se_editor_freeze(ui,passive); se_studio_check(passive.state[13] == -1 && ui.state[1] == 14,"passive panels do not mutate scene UI focus");
            }
            se_studio_check(vmem.commit_total_allocated() == warm,"modal frames have zero commit drift");
            ui.state[42] = 0; ui.state[39] = 0;
            se_studio_check(se_editor_cancel_focus(ui,tools,document) && ui.state[1] == -1 && ui.state[39] == 1 && ui.state[37] == 0,"Escape cancels inspector without exiting in same frame");
            se_record(history,authored);
            se_document_request(ui,authored,history,path,document,1,"");
            se_edit_begin(ui,16,-1,fixture); ui.state[13] = 2;
            se_studio_check(se_editor_cancel_focus(ui,tools,document) && ui.state[34] == 0 && ui.state[13] == 0 && ui.state[37] == 0 && authored.count() == 5 && history.dirty(),"Escape cancels pending path action without mutation or quitting");
            se_document_cancel(ui,document);
            se_clear(authored); se_demo(authored); se_history_reset(history,authored,false);
            authored.property(authored.handle(2),0,123456.789); authored.property(authored.handle(2),1,4.9406564584124654e-324); se_record(history,authored);
            se_edit_begin(ui,16,-1,prefix+"-ordinary.vscene"); ui.state[13] = 2; se_editor_commit(ui,authored,staging,history,bank,directory,path,document);
            se_studio_check(vfs.read(prefix+"-ordinary.vscene") == se_scene_text(authored) && authored.count() == 5,"ordinary scenes retain version1 layout with exact numeric save content");
            se_world_load_scene(runtime,staging,prefix+"-ordinary.vscene");
            se_studio_check(runtime.number(runtime.handle(2),0) == 123456.789 && runtime.number(runtime.handle(2),1) == 4.9406564584124654e-324,"ordinary document save/load preserves fractions and subnormals");
        };
    };
    } finally { app.close(); }
};
se_studio_check(vmem.total_allocated() == before && vraylib.live_handles() == 0,"studio owners and native handles reclaimed");
out("VMGE_STUDIO_OK");
