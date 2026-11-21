use "../src/editor/workspace.vy";
use native vmem;

fn ui_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
fn ui_pixel(target :: vraylib.RenderTarget,x :: Int64,y :: Int64) -> vraylib.Color {
    image :: Int64 := vraylib.load_image_from_texture(target.texture().native_handle());
    colors :: Int64 := vraylib.load_image_colors(image);
    pixel :: vraylib.Color := vraylib.buffer_read(colors,(target.height()-1-y)*target.width()+x);
    vraylib.unload_image_colors(colors); vraylib.unload_image(image); return pixel;
}
app :: vraylib.App := vraylib.app_options(320,160,"VMGE UI focused checks",0,vraylib.FLAG_WINDOW_HIDDEN);
try {
    root :: String := vfs.join(vraylib.get_working_directory(),"examples/projects/vmge");
    se_theme_load(app,root);
    handles :: Int64 := vraylib.live_handles(); se_theme_load(app,root);
    ui_check(vraylib.live_handles() == handles,"theme loading is idempotent");
    bytes :: Array<Int64> := []; bytes.reserve(256); bytes.resize(256,0);
    ui :: SEUI := se_ui(bytes);
    se_edit_begin(ui,14,0,"Aé😀Z");
    ui_check(ui.text[0] == 8,"UTF-8 byte length");
    ui_check(se_edit_previous(ui.text,7) == 3 && se_edit_next(ui.text,3) == 7,"caret respects four-byte UTF-8");
    ui.state[26] = 3; ui.state[27] = 1;
    se_text_write(ui.edit_buffer,"ç"); se_edit_insert(ui,ui.edit_buffer);
    ui_check(se_text_read(ui.text) == "Aç😀Z" && ui.state[26] == 3,"replace selected Unicode text");
    se_edit_delete(ui,3,7); ui_check(se_text_read(ui.text) == "AçZ","delete whole codepoint");
    ui.state[26] = 4; ui.state[27] = 4;
    se_text_write(ui.edit_buffer,"\n"); ui_check(!se_edit_insert(ui,ui.edit_buffer) && se_text_read(ui.text) == "AçZ","clipboard controls rejected atomically");
    se_edit_begin(ui,14,0,"end"); ui.state[26] = 3; ui.state[27] = 0;
    se_text_write(ui.edit_buffer,"Inter"); se_edit_insert(ui,ui.edit_buffer);
    ui_check(se_text_read(ui.text) == "Inter","select all replacement");
    ui_check(se_measure("WWW",14) > se_measure("iii",14),"real proportional font metrics");
    ui_check(se_measure(se_fit("Long Unicode éçğ label",40.0,14),14) <= 40.0,"measured ellipsis fits");
    ui_check(se_ui_asset_relative("C:/project","C:/project/assets/cube.obj") == "assets/cube.obj","project-relative asset path");
    ui_check(se_ui_asset_relative("C:/project","C:/project-other/cube.obj") == "C:/project-other/cube.obj","project path component boundary");
    directory :: Array<Int64> := []; directory.reserve(256); directory.resize(256,0); se_text_write(directory,root);
    file_path :: Array<Int64> := []; file_path.reserve(256); file_path.resize(256,0);
    se_ui_prepare(ui,directory);
    ui_check(ui.state[29] > 0 && ui.state[33] == 1,"preframe listing warmup");
    seen_file :: Bool := false; index :: Int64 := 0;
    while index < ui.state[29] { if ui.asset_kinds[index] == 0 { seen_file = true; } else { ui_check(!seen_file,"folders precede files"); } index = index+1; }
    names :: Array := []; paths :: Array := []; asset_paths :: Array := [];
    index = 0;
    while index < 128 {
        name_bytes :: Array<Int64> := []; name_bytes.reserve(64); name_bytes.resize(64,0); names.push(name_bytes);
        path_bytes :: Array<Int64> := []; path_bytes.reserve(256); path_bytes.resize(256,0); paths.push(path_bytes); index = index+1;
    }
    index = 0; while index < 64 { path_bytes :: Array<Int64> := []; path_bytes.reserve(256); path_bytes.resize(256,0); asset_paths.push(path_bytes); index = index+1; }
    numeric_slots :: Array := []; text_slots :: Array := []; index = 0;
    while index < 17 {
        numeric :: Array<Float64> := []; numeric.reserve(2048); numeric.resize(2048,0.0); numeric_slots.push(numeric);
        text_slot :: Array<Int64> := []; text_slot.reserve(41988); text_slot.resize(41988,0); text_slots.push(text_slot); index = index+1;
    }
    scene :: SEScene := se_scene(names,paths,0);
    history :: SEHistory := se_history(numeric_slots,text_slots);
    bank :: SEAssets := se_assets(app,asset_paths,root);
    parent :: Int64 := scene.spawn(0,"Root",-1); ui.state[0] = parent;
    se_history_reset(history,scene,true);
    ui_check(vfs.is_dir("build"),"fixture output parent exists");
    fixture :: String := vfs.join(vraylib.get_working_directory(),"build/vmge-ui-asset-fixture-20261121.png");
    image :: vraylib.Image := app.image(1,1,vraylib.WHITE); image.save(fixture); image.close();
    se_asset_assign(ui,scene,history,bank,fixture,true);
    mesh :: Int64 := ui.state[0];
    ui_check(scene.count() == 2 && scene.kind(mesh) == 1 && scene.parent(mesh) == parent,"asset creates a real parented mesh");
    ui_check(scene.path(mesh) == se_ui_asset_relative(root,fixture) && history.dirty(),"asset assignment is recorded");
    se_asset_assign(ui,scene,history,bank,fixture,false);
    ui_check(scene.count() == 2 && bank.state[0] == 1,"assign reuses the loaded model");
    ui_check(!se_asset_use(ui,scene,history,bank,fixture,false) && ui.browser_state[2] == 1 && scene.count() == 2,"external references require deliberate confirmation");
    ui_check(se_asset_use(ui,scene,history,bank,fixture,false) && ui.browser_state[2] == 0,"second explicit action confirms external assignment");
    failed :: Bool := false;
    try { se_asset_assign(ui,scene,history,bank,fixture+".missing.obj",true); } catch (error) { failed = true; }
    ui_check(failed && scene.count() == 2 && ui.state[0] == mesh,"failed load cannot create a partial node");
    target :: vraylib.RenderTarget := app.target(320,160);
    warm :: Int64 := vmem.commit_total_allocated(); app.limit(4);
    while app.frame() {
        target.begin(); app.clear(vraylib.BLACK);
        panel :: vraylib.Rectangle := se_rect(10.0,10.0,200.0,100.0);
        clip :: vraylib.Clip := app.clip(panel); se_clip_start(clip,panel);
        try {
            se_panel(panel,"Inter UI");
            se_edit_begin(ui,14,0,"Caret éçğ"); se_field("Name",se_text_read(ui.text),se_rect(15.0,40.0,180.0,24.0),true,true);
            # A field's temporary scissor must restore the containing panel.
            vraylib.draw_rectangle(0,85,320,8,vraylib.RED);
        } finally { se_clip_finish(clip); }
        se_draw_text("Regular / éçğ",15,120,14,vraylib.WHITE);
        if app.frames() == 1 { se_text_write(directory,vfs.join(root,"assets/fonts")); }
        if app.frames() == 2 { ui.state[32] = 1; }
        ui.state[1] = -1;
        se_asset_panel(app,ui,scene,history,bank,directory,file_path,se_rect(215.0,0.0,105.0,160.0));
        expected :: Int64 := 3; if app.frames() == 0 { expected = 1; } if app.frames() == 1 { expected = 2; }
        ui_check(ui.state[33] == expected,"listing scans only on navigation/refresh");
        if app.frames() >= 1 { ui_check(se_contains(se_asset_name(ui,0),"Inter") || se_contains(se_asset_name(ui,1),"Inter"),"cached filenames survive poisoned frames"); }
        target.end();
        inside :: vraylib.Color := ui_pixel(target,20,88); outside :: vraylib.Color := ui_pixel(target,212,88);
        ui_check(inside.r == 230 && outside.r == 0,"field clipping restores panel scissor");
        app.begin(); target.present(320.0,160.0); app.end();
        ui_check(app.passes() == 0,"balanced UI passes");
    }
    ui_check(vmem.commit_total_allocated() == warm,"UI retains no frame-created boxes");
} finally { app.close(); }
ui_check(vraylib.live_handles() == 0,"theme resources close with app");
out("VMGE_UI_OK");
