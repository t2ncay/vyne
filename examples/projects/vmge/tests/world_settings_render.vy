use "../src/editor/world_tools.vy";
use native vmem;
fn world_controls_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.app_options(900,600,"world direct controls",0,vraylib.FLAG_WINDOW_HIDDEN);
@bounded<16777216> region world_controls_owner {
    se_theme_load(app,vmge_project_root()); tools :: SEWorldTools := se_world_tools();
    prefix :: String := vmge_option("VYNE_VMGE_WORLD_CONTROLS_PREFIX","build/world-dev-controls");
    committed :: Int64 := vmem.commit_total_allocated(); warm :: Int64 := vmem.total_allocated(); app.limit(6);
    try {
        while app.frame() {
            tick :: Int64 := app.frames(); tick = tick+1;
            if tick == 1 { se_world_settings_begin(tools.settings,tools.numbers); se_edit_begin(tools.settings.editor,0,-1,"42"); }
            if tick == 3 { se_world_settings_picker(tools.settings,2,5); }
            if tick == 5 { se_world_settings_picker(tools.settings,3,1); }
            world_controls_check(se_world_tools_modal_open(tools) && se_world_tools_busy(tools),"modal captures world interaction");
            se_world_tools_modal_input(tools);
            app.begin(); app.clear(vraylib.rgba(19,23,29,255));
            try { se_world_tools_modal_draw(app,tools); } finally { app.end(); }
            # Desktop screenshot reads the completed previous back buffer.
            if tick == 2 { vraylib.take_screenshot(prefix+"-settings.png"); }
            if tick == 4 { vraylib.take_screenshot(prefix+"-tools.png"); }
            if tick == 6 { vraylib.take_screenshot(prefix+"-presets.png"); }
            world_controls_check(app.passes() == 0 && tools.settings.editor.text.size() == 128 && tools.settings.numeric.size() == 160,"balanced modal drawing and fixed editing storage");
        }
        world_controls_check(vmem.total_allocated()-warm <= 512 && vmem.commit_total_allocated() == committed,"modal controls retain no frame data");
    } finally { app.close(); }
};
world_controls_check(vraylib.live_handles() == 0,"modal graphics cleanup"); out("VMGE_WORLD_SETTINGS_RENDER_OK");
