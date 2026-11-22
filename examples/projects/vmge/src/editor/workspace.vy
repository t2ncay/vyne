use "viewport.vy";
use "../ui/assets.vy";
use "../scene/history.vy";
use "../scene/io.vy";
use "../scene/play.vy";
use native vmem;

fn se_schema() -> Array {
    labels :: Array := [];
    @persistent region immutable_schema {
        labels = ["Position X","Position Y","Position Z","Rotation X","Rotation Y","Rotation Z","Scale X","Scale Y","Scale Z","Red","Green","Blue","Alpha","FOV"];
    };
    return labels;
}
fn se_message(logs :: Array,state :: Array<Int64>,message :: String) -> Bool {
    slot :: Int64 := state[0] % 32;
    # Error messages can contain long source paths. Keep the complete message
    # in console output, and fit the display slot without changing scene data.
    if message.size() < 256 { se_text_write(logs[slot],message); }
    else { se_text_write(logs[slot],"Operation failed; see console for details"); }
    state[0] = state[0]+1;
    out(message);
    return true;
}
fn se_choose(ui :: SEUI,scene :: SEScene,history :: SEHistory,id :: Int64) -> Bool {
    if ui.state[6] != -1 {
        scene.reparent(ui.state[6],id); se_record(history,scene); ui.state[6] = -1;
    } else { ui.state[0] = id; }
    return true;
}
fn se_tree_rows(app :: vraylib.App,ui :: SEUI,scene :: SEScene,history :: SEHistory,parent :: Int64,depth :: Int64,row :: Array<Int64>,rectangle :: vraylib.Rectangle,editable :: Bool) -> Bool {
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot*8] != 0 {
            if scene.meta[slot*8+2] == parent {
                id :: Int64 := scene.handle(slot);
                query :: String := se_text_read(ui.tree_search);
                matches :: Bool := query == "" || se_contains(scene.name(id),query);
                y :: Float64 := rectangle.y+170.0+float64(row[0]-ui.state[7])*24.0;
                if matches && y >= rectangle.y+168.0 && y < rectangle.y+rectangle.height-20.0 {
                    item :: vraylib.Rectangle := se_rect(rectangle.x+4.0,y,rectangle.width-8.0,21.0);
                    if ui.state[0] == id { vraylib.draw_rectangle_rec(item,vraylib.rgba(47,76,110,255)); }
                    indent :: Float64 := rectangle.x+8.0+float64(depth)*12.0;
                    fold :: vraylib.Rectangle := se_rect(indent,y,17.0,21.0);
                    children :: Bool := se_has_children(scene,id);
                    if children { label :: String := "-"; if ui.collapsed[slot] == id { label = "+"; } se_draw_text(label,int64(indent+3.0),int64(y+3.0),14,vraylib.SKYBLUE); }
                    se_draw_text(se_fit(scene.name(id),rectangle.x+rectangle.width-indent-30.0,14),int64(indent+19.0),int64(y+3.0),14,vraylib.LIGHTGRAY);
                    if se_hover(item) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) && editable && ui.state[1] == -1 {
                        if children && se_hover(fold) { if ui.collapsed[slot] == id { ui.collapsed[slot] = -1; } else { ui.collapsed[slot] = id; } }
                        else { se_choose(ui,scene,history,id); }
                    }
                }
                if matches { row[0] = row[0]+1; }
                if ui.collapsed[slot] != id || query != "" { se_tree_rows(app,ui,scene,history,id,depth+1,row,rectangle,editable); }
            }
        }
        slot = slot + 1;
    }
    return true;
}
fn se_tree(app :: vraylib.App,ui :: SEUI,scene :: SEScene,history :: SEHistory,work :: Array<Int64>,rectangle :: vraylib.Rectangle) -> Bool {
    se_panel(rectangle,"Scene");
    editable :: Bool := ui.state[4] == 0 && ui.state[13] == 0 && ui.state[47] == 0;
    clip :: vraylib.Clip := app.clip(rectangle); se_clip_start(clip,rectangle);
    try {
    if se_button("+ Add Node...",se_rect(rectangle.x+5.0,rectangle.y+30.0,rectangle.width-10.0,27.0),editable && ui.state[1] == -1) { se_node_picker_open(ui); }
    if editable && ui.state[1] == -1 && se_hover(rectangle) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_RIGHT) { se_node_picker_open(ui); }
    selected :: Bool := scene.valid(ui.state[0]);
    if se_button("Duplicate",se_rect(rectangle.x+5.0,rectangle.y+62.0,(rectangle.width-15.0)*0.5,27.0),editable && selected) { ui.state[0] = se_duplicate(scene,ui.state[0],work); se_record(history,scene); }
    if se_button("Delete",se_rect(rectangle.x+10.0+(rectangle.width-15.0)*0.5,rectangle.y+62.0,(rectangle.width-15.0)*0.5,27.0),editable && selected) { se_delete(scene,ui.state[0],work); ui.state[0] = -1; se_record(history,scene); }
    if se_button("Reparent",se_rect(rectangle.x+5.0,rectangle.y+94.0,(rectangle.width-15.0)*0.5,27.0),editable && selected) { ui.state[6] = ui.state[0]; }
    if se_button("To root",se_rect(rectangle.x+10.0+(rectangle.width-15.0)*0.5,rectangle.y+94.0,(rectangle.width-15.0)*0.5,27.0),editable && selected) { scene.reparent(ui.state[0],-1); ui.state[6] = -1; se_record(history,scene); }
    query :: String := se_text_read(ui.tree_search); if ui.state[1] == 18 { query = se_text_read(ui.text); }
    if se_field("Search",query,se_rect(rectangle.x+6.0,rectangle.y+133.0,rectangle.width-12.0,25.0),ui.state[1] == 18,editable) { se_edit_begin(ui,18,-1,se_text_read(ui.tree_search)); }
    # Reuse the caller's fixed work buffer for row counting.
    work[0] = 0;
    if query == "" || se_contains("World map terrain",query) {
        y :: Float64 := rectangle.y+170.0-float64(ui.state[7])*24.0;
        if y >= rectangle.y+168.0 && y < rectangle.y+rectangle.height-20.0 {
            item :: vraylib.Rectangle := se_rect(rectangle.x+4.0,y,rectangle.width-8.0,21.0);
            if ui.state[0] == -2 { vraylib.draw_rectangle_rec(item,vraylib.rgba(47,76,110,255)); }
            se_draw_heading(se_fit("World / map + terrain",rectangle.width-24.0,13),int64(rectangle.x+12.0),int64(y+3.0),13,vraylib.SKYBLUE);
            if editable && ui.state[1] == -1 && ui.state[6] == -1 && se_hover(item) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) { ui.state[0] = -2; }
        }
        work[0] = 1;
    }
    se_tree_rows(app,ui,scene,history,-1,0,work,rectangle,editable);
    visible :: Float64 := vmath.max(1.0,vmath.floor((rectangle.height-190.0)/24.0));
    if se_hover(rectangle) { ui.state[7] = int64(vmath.clamp(float64(ui.state[7])-vraylib.get_mouse_wheel_move()*3.0,0.0,vmath.max(0.0,float64(work[0])-visible))); }
    se_draw_text(string(scene.count())+" nodes | World resource",int64(rectangle.x+8.0),int64(rectangle.y+rectangle.height-17.0),12,vraylib.GRAY);
    } finally { se_clip_finish(clip); }
    return true;
}
fn se_inspector(app :: vraylib.App,ui :: SEUI,scene :: SEScene,history :: SEHistory,schema :: Array,rectangle :: vraylib.Rectangle) -> Bool {
    se_panel(rectangle,"Inspector");
    if ui.state[0] == -2 { return se_world_inspector(app,ui,scene,rectangle); }
    if !scene.valid(ui.state[0]) { se_draw_text("Select a node to inspect",int64(rectangle.x+8.0),int64(rectangle.y+40.0),14,vraylib.GRAY); return true; }
    editable :: Bool := ui.state[4] == 0 && ui.state[13] == 0 && ui.state[47] == 0;
    id :: Int64 := ui.state[0]; slot :: Int64 := id%128;
    body :: vraylib.Rectangle := se_rect(rectangle.x+1.0,rectangle.y+27.0,rectangle.width-2.0,rectangle.height-28.0);
    clip :: vraylib.Clip := app.clip(body); se_clip_start(clip,body);
    try {
    top :: Float64 := rectangle.y+34.0-float64(ui.state[24])*27.0;
    se_draw_heading(se_type_name(scene.kind(id)),int64(rectangle.x+8.0),int64(top),14,vraylib.SKYBLUE);
    name :: String := scene.name(id); if ui.state[1] == 14 { name = se_text_read(ui.text); }
    if se_field("Name",name,se_rect(rectangle.x+6.0,top+23.0,rectangle.width-12.0,24.0),ui.state[1] == 14,editable) { se_edit_begin(ui,14,id,scene.name(id)); }
    path :: String := scene.path(id); if ui.state[1] == 15 { path = se_text_read(ui.text); }
    next :: Float64 := top+50.0;
    if scene.kind(id) == 1 {
        if se_field("Asset",path,se_rect(rectangle.x+6.0,next,rectangle.width-12.0,24.0),ui.state[1] == 15,editable) { se_edit_begin(ui,15,id,scene.path(id)); }
        next = next+27.0;
        material_path :: String := se_text_read(scene.materials.paths[slot]);
        material_label :: String := "Shader: Open Studio";
        if material_path != "" { material_label = "Shader: "+vfs.basename(material_path); }
        if se_button(se_fit(material_label,rectangle.width-82.0,13),se_rect(rectangle.x+6.0,next,rectangle.width-70.0,25.0),editable && ui.state[1] == -1) { ui.state[45] = 1; }
        if se_button("Clear",se_rect(rectangle.x+rectangle.width-58.0,next,52.0,25.0),editable && material_path != "") { bytes :: Array<Int64> := scene.materials.paths[slot]; bytes[0] = 0; se_record(history,scene); }
        next = next+29.0;
    }
    if scene.kind(id) == 2 {
        if se_field("Pipeline",path,se_rect(rectangle.x+6.0,next,rectangle.width-12.0,24.0),ui.state[1] == 15,editable) { se_edit_begin(ui,15,id,scene.path(id)); }
        next = next+28.0;
        if se_button("Use VHS preset",se_rect(rectangle.x+6.0,next,rectangle.width-12.0,25.0),editable && ui.state[1] == -1) { se_text_write(scene.paths[scene.offset+slot],"shaders/camera/vhs.vpipeline"); se_record(history,scene); }
        next = next+29.0;
        effect_label :: String := "Effects: enabled"; if scene.extra[slot*16] == 0.0 { effect_label = "Effects: bypass"; }
        if se_button(effect_label,se_rect(rectangle.x+6.0,next,rectangle.width-12.0,25.0),editable) { se_extra_set(scene,id,0,1.0-scene.extra[slot*16]); se_record(history,scene); }
        next = next+29.0;
        preview_label :: String := "Preview camera"; if ui.state[53] != 0 { preview_label = "Back to editor view"; }
        if se_button(preview_label,se_rect(rectangle.x+6.0,next,rectangle.width-12.0,25.0),editable) { se_make_active(scene,id); ui.state[53] = 1-ui.state[53]; se_record(history,scene); }
        next = next+29.0;
        if se_button("Edit pipeline / camera input",se_rect(rectangle.x+6.0,next,rectangle.width-12.0,25.0),editable && path != "") { se_make_active(scene,id); ui.state[52] = 1; se_record(history,scene); }
        next = next+29.0;
        if se_button("Reload camera effects",se_rect(rectangle.x+6.0,next,rectangle.width-12.0,25.0),editable && path != "") { ui.state[55] = 1; }
        next = next+29.0;
    }
    if scene.kind(id) == 2 || scene.kind(id) == 5 {
        active_label :: String := "Make active"; if se_active_node(scene,scene.kind(id)) == id { active_label = "Active"; }
        if se_button(active_label,se_rect(rectangle.x+6.0,next,rectangle.width-12.0,25.0),editable) { se_make_active(scene,id); se_record(history,scene); }
        next = next+29.0;
    }
    visible_label :: String := "Hidden"; if scene.meta[slot*8+4] != 0 { visible_label = "Visible"; }
    if se_tool_button(visible_label,se_rect(rectangle.x+6.0,next,rectangle.width*0.5-8.0,25.0),editable,scene.meta[slot*8+4] != 0,"Toggle node visibility") { scene.meta[slot*8+4] = 1-scene.meta[slot*8+4]; se_record(history,scene); }
    if scene.kind(id) == 1 { if se_button("Material " + string(scene.meta[slot*8+5]+1),se_rect(rectangle.x+rectangle.width*0.5,next,rectangle.width*0.5-6.0,25.0),editable) { scene.meta[slot*8+5] = (scene.meta[slot*8+5]+1)%8; se_record(history,scene); } }
    next = next+34.0;
    se_inspector_group(ui,scene,schema,id,"Position",0,3,next,rectangle,editable); next = next+105.0;
    se_inspector_group(ui,scene,schema,id,"Rotation / degrees",3,3,next,rectangle,editable); next = next+105.0;
    se_inspector_group(ui,scene,schema,id,"Scale",6,3,next,rectangle,editable); next = next+105.0;
    if scene.kind(id) == 1 { se_inspector_group(ui,scene,schema,id,"Appearance / RGBA",9,4,next,rectangle,editable); next = next+132.0; }
    if scene.kind(id) == 2 { se_inspector_group(ui,scene,schema,id,"Camera / perspective",13,1,next,rectangle,editable); next = next+51.0; }
    if scene.kind(id) == 6 || scene.kind(id) == 7 { se_inspector_group(ui,scene,schema,id,"Light color / RGBA",9,4,next,rectangle,editable); next = next+132.0; }
    if scene.kind(id) >= 5 {
        count :: Int64 := 3; if scene.kind(id) == 5 { count = 11; }
        se_draw_heading("Environment",int64(rectangle.x+8.0),int64(next),13,vraylib.SKYBLUE); next = next+22.0;
        f :: Int64 := 0; while f < count {
            value :: String := string(scene.extra[slot*16+f]); if ui.state[1] == 64+f { value = se_text_read(ui.text); }
            if se_field(se_extra_label(scene.kind(id),f),value,se_rect(rectangle.x+6.0,next,rectangle.width-12.0,24.0),ui.state[1] == 64+f,editable) { se_edit_begin(ui,64+f,id,string(scene.extra[slot*16+f])); }
            next = next+27.0; f = f+1;
        }
    }
    content :: Float64 := next-top+12.0;
    if se_hover(rectangle) { ui.state[24] = int64(vmath.clamp(float64(ui.state[24])-vraylib.get_mouse_wheel_move()*3.0,0.0,vmath.max(0.0,vmath.ceil((content-body.height)/27.0)))); }
    } finally { se_clip_finish(clip); }
    return true;
}
fn se_world_inspector(app :: vraylib.App,ui :: SEUI,scene :: SEScene,rectangle :: vraylib.Rectangle) -> Bool {
    body :: vraylib.Rectangle := se_rect(rectangle.x+1.0,rectangle.y+27.0,rectangle.width-2.0,rectangle.height-28.0);
    clip :: vraylib.Clip := app.clip(body); se_clip_start(clip,body);
    try {
        x :: Int64 := int64(rectangle.x+8.0); y :: Int64 := int64(rectangle.y+36.0);
        se_draw_heading("World resource",x,y,16,vraylib.SKYBLUE);
        se_draw_text("Map cells: "+string(scene.world.count())+" / 4096",x,y+32,14,vraylib.WHITE);
        se_draw_text("Terrain: "+string(scene.world.side())+" x "+string(scene.world.side())+" quads",x,y+57,14,vraylib.WHITE);
        se_draw_text("Spacing: "+string(scene.world.data[2]),x,y+82,14,vraylib.LIGHTGRAY);
        se_draw_text("Seed: "+string(int64(scene.world.data[6])),x,y+107,14,vraylib.LIGHTGRAY);
        se_draw_text("Height: "+string(scene.world.data[8]),x,y+132,14,vraylib.LIGHTGRAY);
        if se_button("World settings",se_rect(rectangle.x+8.0,rectangle.y+192.0,rectangle.width-16.0,29.0),ui.state[4] == 0 && ui.state[13] == 0 && ui.state[1] == -1) { ui.state[44] = 1; }
        se_draw_text(se_fit("Paint cells or sculpt terrain in the viewport.",rectangle.width-16.0,13),x,y+202,13,vraylib.GRAY);
        se_draw_text(se_fit("One undo step per stroke. F6 to Play.",rectangle.width-16.0,13),x,y+225,13,vraylib.GRAY);
    } finally { se_clip_finish(clip); }
    return true;
}
fn se_inspector_group(ui :: SEUI,scene :: SEScene,schema :: Array,id :: Int64,title :: String,start :: Int64,count :: Int64,top :: Float64,rectangle :: vraylib.Rectangle,editable :: Bool) -> Bool {
    se_draw_heading(title,int64(rectangle.x+8.0),int64(top),13,vraylib.rgba(169,193,227,255));
    field :: Int64 := start;
    while field < start+count {
        value :: String := string(scene.number(id,field)); if ui.state[1] == field { value = se_text_read(ui.text); }
        item :: vraylib.Rectangle := se_rect(rectangle.x+6.0,top+20.0+float64(field-start)*27.0,rectangle.width-12.0,24.0);
        if item.y >= rectangle.y+27.0 && item.y+item.height <= rectangle.y+rectangle.height { if se_field(schema[field],value,item,ui.state[1] == field,editable) { se_edit_begin(ui,field,id,string(scene.number(id,field))); } }
        field = field+1;
    }
    return true;
}
fn se_has_children(scene :: SEScene,id :: Int64) -> Bool {
    i :: Int64 := 0; while i < 128 { if scene.meta[i*8] != 0 && scene.meta[i*8+2] == id { return true; } i = i+1; } return false;
}
fn se_contains(text :: String,query :: String) -> Bool {
    haystack :: String := vraylib.text_to_lower(text); needle :: String := vraylib.text_to_lower(query);
    i :: Int64 := 0;
    while i+needle.size() <= haystack.size() {
        j :: Int64 := 0; matches :: Bool := true;
        while j < needle.size() { if haystack[i+j] != needle[j] { matches = false; } j = j+1; }
        if matches { return true; } i = i+1;
    }
    return false;
}
fn se_asset_supported(path :: String) -> Bool {
    ext :: String := vraylib.text_to_lower(vfs.extension(path));
    return ext == ".obj" || ext == ".glb" || ext == ".gltf" || ext == ".iqm" || ext == ".m3d" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
}
fn se_asset_panel(app :: vraylib.App,ui :: SEUI,scene :: SEScene,history :: SEHistory,bank :: SEAssets,directory :: Array<Int64>,scene_path :: Array<Int64>,rectangle :: vraylib.Rectangle) -> Bool {
    se_panel(rectangle,"Assets");
    editable :: Bool := ui.state[4] == 0 && ui.state[13] == 0 && ui.state[1] == -1;
    clip :: vraylib.Clip := app.clip(rectangle); se_clip_start(clip,rectangle);
    try {
    path :: String := se_text_read(directory);
    if se_button("Up",se_rect(rectangle.x+6.0,rectangle.y+29.0,40.0,25.0),editable) { se_text_write(directory,vfs.dirname(path)); ui.state[8] = 0; }
    if se_button("Refresh",se_rect(rectangle.x+52.0,rectangle.y+29.0,67.0,25.0),editable) { ui.state[32] = 1; se_assets_retry(bank); }
    current :: String := path; if ui.state[1] == 17 { current = se_text_read(ui.text); }
    if se_field("Directory",current,se_rect(rectangle.x+124.0,rectangle.y+29.0,rectangle.width-130.0,25.0),ui.state[1] == 17,ui.state[4] == 0 && ui.state[13] == 0) { se_edit_begin(ui,17,-1,path); }
    path = se_text_read(directory);
    if path != se_text_read(ui.asset_directory) || ui.state[32] != 0 {
        selected_name :: String := "";
        if ui.state[32] != 0 {
            if path == se_text_read(ui.asset_directory) && ui.state[28] >= 0 && ui.state[28] < ui.state[29] { selected_name = se_asset_name(ui,ui.state[28]); }
            se_preview_invalidate(ui,app);
        }
        se_asset_listing(ui,path);
        if selected_name != "" { se_asset_select_name(ui,app,path,selected_name); }
    }
    query :: String := se_text_read(ui.asset_search); if ui.state[1] == 19 { query = se_text_read(ui.text); }
    sidebar :: Float64 := 0.0; if rectangle.width >= 460.0 { sidebar = 240.0; }
    list_width :: Float64 := rectangle.width-sidebar-12.0;
    search_width :: Float64 := list_width;
    if list_width >= 280.0 { search_width = list_width*0.46; }
    if se_field("Search",query,se_rect(rectangle.x+6.0,rectangle.y+59.0,search_width,25.0),ui.state[1] == 19,ui.state[4] == 0 && ui.state[13] == 0) { se_edit_begin(ui,19,-1,se_text_read(ui.asset_search)); }
    if search_width < list_width { se_asset_breadcrumb(directory,bank.root,path,se_rect(rectangle.x+12.0+search_width,rectangle.y+59.0,list_width-search_width-6.0,25.0),editable); }
    if sidebar > 0.0 { if se_button("Project root",se_rect(rectangle.x+rectangle.width-sidebar+4.0,rectangle.y+59.0,sidebar-10.0,25.0),editable) { se_text_write(directory,bank.root); } }
    list_bottom :: Float64 := rectangle.y+rectangle.height-24.0;
    if sidebar == 0.0 { list_bottom = list_bottom-29.0; }
    visible :: Float64 := vmath.max(0.0,vmath.floor((list_bottom-rectangle.y-89.0)/24.0));
    rows :: Int64 := 0; i :: Int64 := 0;
    while i < ui.state[29] {
        name :: String := se_asset_name(ui,i);
        if se_contains(name,query) {
            y :: Float64 := rectangle.y+89.0+float64(rows-ui.state[8])*24.0;
            if y >= rectangle.y+89.0 && y+22.0 <= list_bottom {
                item :: vraylib.Rectangle := se_rect(rectangle.x+6.0,y,list_width,22.0);
                label :: String := name; if ui.asset_kinds[i] == 1 { label = "+  "+name+"/"; }
                if ui.state[28] == i { vraylib.draw_rectangle_rec(item,vraylib.rgba(44,70,106,255)); }
                else { if se_hover(item) { vraylib.draw_rectangle_rec(item,vraylib.rgba(37,45,59,255)); } }
                tint :: vraylib.Color := vraylib.LIGHTGRAY; if ui.asset_kinds[i] == 1 { tint = vraylib.SKYBLUE; }
                se_draw_text(se_fit(label,list_width-12.0,14),int64(item.x+6.0),int64(y+4.0),14,tint);
                if editable && se_hover(item) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) {
                    ui.browser_state[2] = 0; ui.browser_state[3] = -1; ui.external_path[0] = 0; ui.asset_notice[0] = 0;
                    if ui.asset_kinds[i] == 1 { se_text_write(directory,vfs.join(path,name)); ui.state[8] = 0; ui.state[28] = -1; }
                    else { ui.state[28] = i; se_preview_request(ui,app,vfs.join(path,name)); }
                }
            }
            rows = rows+1;
        }
        i = i+1;
    }
    if se_hover(se_rect(rectangle.x,rectangle.y+85.0,list_width+12.0,vmath.max(0.0,list_bottom-rectangle.y-85.0))) { ui.state[8] = int64(vmath.clamp(float64(ui.state[8])-vraylib.get_mouse_wheel_move()*3.0,0.0,vmath.max(0.0,float64(rows)-visible))); }
    if ui.state[28] >= 0 && ui.state[28] < ui.state[29] {
        name :: String := se_asset_name(ui,ui.state[28]); full :: String := vfs.join(path,name);
        x :: Float64 := rectangle.x+rectangle.width-sidebar+4.0; y :: Float64 := rectangle.y+91.0; width :: Float64 := sidebar-10.0;
        if sidebar == 0.0 { x = rectangle.x+6.0; y = list_bottom+2.0; width = (rectangle.width-24.0)/3.0; }
        if sidebar > 0.0 {
            se_draw_heading(se_fit(name,width,13),int64(x),int64(y),13,vraylib.WHITE); y = y+16.0;
            se_preview_draw(ui,app,full,se_rect(x,y,64.0,64.0)); x = x+70.0; width = width-70.0;
        }
        if se_asset_supported(full) {
            assign :: Bool := scene.valid(ui.state[0]); if assign { assign = scene.kind(ui.state[0]) == 1; }
            external_reference :: Bool := vraylib.is_path_absolute(se_ui_asset_relative(bank.root,full));
            assign_label :: String := "Assign to mesh"; create_label :: String := "New mesh node";
            if external_reference { assign_label = "Use external mesh"; create_label = "New external mesh"; }
            if ui.browser_state[2] == 1 && ui.browser_state[3] == ui.state[0] && se_text_read(ui.external_path) == full { assign_label = "Confirm external"; }
            if ui.browser_state[2] == 2 && ui.browser_state[3] == ui.state[0] && se_text_read(ui.external_path) == full { create_label = "Confirm external"; }
            if se_button(assign_label,se_rect(x,y,width,20.0),editable && assign) { se_asset_use(ui,scene,history,bank,full,false); }
            if sidebar == 0.0 { x = x+width+6.0; } else { y = y+23.0; }
            if se_button(create_label,se_rect(x,y,width,20.0),editable) { se_asset_use(ui,scene,history,bank,full,true); }
            if sidebar == 0.0 { x = x+width+6.0; } else { y = y+23.0; }
            if se_button("Import into project",se_rect(x,y,width,20.0),editable) { se_asset_import_ui(ui,app,bank.root,full,directory); }
            if vfs.extension(full) == ".png" || vfs.extension(full) == ".jpg" || vfs.extension(full) == ".jpeg" || vfs.extension(full) == ".bmp" {
                if sidebar > 0.0 { y = y+23.0; }
                if sidebar > 0.0 && se_button("Bind studio sampler",se_rect(x,y,width,20.0),editable) { se_text_write(ui.external_path,full); ui.state[45] = 3; }
            }
        } else {
            ext :: String := vraylib.text_to_lower(vfs.extension(full));
            if ext == ".vscene" || ext == ".dat" { label :: String := "Open scene"; if ext == ".dat" { label = "Import legacy map"; }
                if se_button(label,se_rect(x,y,width,24.0),editable) { se_edit_begin(ui,16,-1,full); ui.state[13] = 1; if ext == ".dat" { ui.state[13] = 3; } }
            } else { if ext == ".vshader" || ext == ".vmaterial" {
                if se_button("Open in Shader Studio",se_rect(x,y,width,24.0),editable) { se_text_write(ui.external_path,full); ui.state[45] = 2; }
            } else { se_draw_text("No loader for this file",int64(x),int64(y),12,vraylib.GRAY); } }
        }
    } else { if sidebar > 0.0 { se_draw_text("Select an asset",int64(rectangle.x+rectangle.width-sidebar+4.0),int64(rectangle.y+95.0),13,vraylib.GRAY); } }
    footer :: String := string(rows)+" matches  /  "+string(ui.state[29])+" cached";
    if ui.state[31] != 0 { footer = footer+"  |  listing limited to 512 entries / 255-byte names"; }
    if ui.asset_notice[0] != 0 { footer = se_text_read(ui.asset_notice); }
    se_draw_text(se_fit(footer,rectangle.width-16.0,12),int64(rectangle.x+8.0),int64(rectangle.y+rectangle.height-17.0),12,vraylib.GRAY);
    } finally { se_clip_finish(clip); }
    return true;
}
fn se_asset_name(ui :: SEUI,index :: Int64) -> String {
    offset :: Int64 := index*256; builder :: Int64 := vcore.sb_create(); i :: Int64 := 1;
    while i <= ui.asset_names[offset] { vcore.sb_append_char(builder,ui.asset_names[offset+i]); i = i+1; }
    return vcore.sb_build(builder);
}
fn se_asset_listing(ui :: SEUI,path :: String) -> Bool {
    if !vfs.is_dir(path) { throw "Asset directory not found: "+path; }
    if path.size() >= ui.asset_directory.size() { throw "Asset directory exceeds 255 UTF-8 bytes"; }
    entries :: Array := vfs.list_dir(path);
    # Validate/list first; copy only bytes into fixed, preframe-owned storage.
    ui.state[29] = 0; ui.state[31] = 0; ui.state[28] = -1; ui.state[8] = 0;
    ui.browser_state[2] = 0; ui.browser_state[3] = -1; ui.external_path[0] = 0;
    pass :: Int64 := 1;
    while pass >= 0 {
        i :: Int64 := 0;
        while i < entries.size() {
            name :: String := entries[i]; kind :: Int64 := 0; if vfs.is_dir(vfs.join(path,name)) { kind = 1; }
            if kind == pass && name != "." && name != ".." {
                if ui.state[29] < 512 && name.size() < 256 {
                    se_text_write(ui.edit_buffer,name); offset :: Int64 := ui.state[29]*256; j :: Int64 := 0;
                    while j <= ui.edit_buffer[0] { ui.asset_names[offset+j] = ui.edit_buffer[j]; j = j+1; }
                    ui.asset_kinds[ui.state[29]] = kind; ui.state[29] = ui.state[29]+1;
                } else { ui.state[31] = 1; }
            }
            i = i+1;
        }
        pass = pass-1;
    }
    se_text_write(ui.asset_directory,path); ui.state[32] = 0; ui.state[33] = ui.state[33]+1; return true;
}
fn se_asset_select_name(ui :: SEUI,app :: vraylib.App,directory :: String,name :: String) -> Bool {
    i :: Int64 := 0; while i < ui.state[29] {
        if ui.asset_kinds[i] == 0 && se_asset_name(ui,i) == name { ui.state[28] = i; se_preview_request(ui,app,vfs.join(directory,name)); return true; }
        i = i+1;
    }
    return false;
}
fn se_asset_import_ui(ui :: SEUI,app :: vraylib.App,root :: String,source :: String,directory :: Array<Int64>) -> Bool {
    try {
        relative :: String := se_import_asset(root,source);
        imports :: String := vfs.join(root,"assets/imports"); se_text_write(directory,imports);
        se_asset_listing(ui,imports); se_asset_select_name(ui,app,imports,vfs.basename(relative));
        se_text_write(ui.asset_notice,"Imported project-owned copy; Assign or New mesh to use it"); return true;
    } catch (error) {
        message :: String := string(error); if message.size() < 256 { se_text_write(ui.asset_notice,message); } else { se_text_write(ui.asset_notice,"Import rejected; see Output/console for dependency details"); }
        throw error;
    }
}
fn se_asset_use(ui :: SEUI,scene :: SEScene,history :: SEHistory,bank :: SEAssets,path :: String,create :: Bool) -> Bool {
    full :: String := path; if !vraylib.is_path_absolute(full) { full = vfs.join(vraylib.get_working_directory(),full); }
    relative :: String := se_ui_asset_relative(bank.root,full);
    if relative.size() >= 256 { throw "Asset path exceeds 255 UTF-8 bytes"; }
    action :: Int64 := 1; if create { action = 2; }
    if vraylib.is_path_absolute(relative) && (ui.browser_state[2] != action || ui.browser_state[3] != ui.state[0] || se_text_read(ui.external_path) != full) {
        se_text_write(ui.external_path,full); ui.browser_state[2] = action; ui.browser_state[3] = ui.state[0];
        se_text_write(ui.asset_notice,"External reference is not portable. Click Confirm external again, or Import into project."); return false;
    }
    se_asset_assign(ui,scene,history,bank,full,create);
    ui.browser_state[2] = 0; ui.browser_state[3] = -1; ui.external_path[0] = 0; ui.asset_notice[0] = 0; return true;
}
fn se_asset_assign(ui :: SEUI,scene :: SEScene,history :: SEHistory,bank :: SEAssets,full :: String,create :: Bool) -> Bool {
    relative :: String := se_ui_asset_relative(bank.root,full);
    # Reject oversize paths and load before mutating the scene: errors need no rollback.
    if relative.size() >= 256 { throw "Asset path exceeds 255 UTF-8 bytes"; }
    se_asset(bank,relative);
    id :: Int64 := ui.state[0];
    if create { parent :: Int64 := -1; if scene.valid(id) { parent = id; } name :: String := vfs.stem(full); if name == "" || name.size() >= 64 { name = "MeshInstance3D"; } id = scene.spawn(1,name,parent); }
    if !scene.valid(id) || scene.kind(id) != 1 { throw "Select a MeshInstance3D to assign an asset"; }
    se_text_write(scene.paths[scene.offset+id%128],relative); ui.state[0] = id; se_record(history,scene); return true;
}
fn se_ui_asset_relative(root :: String,full :: String) -> String {
    # A component boundary prevents /project-other from matching /project.
    prefix :: String := ""; i :: Int64 := 0;
    while i < root.size() { ch :: String := root[i]; if ch == "\\" { ch = "/"; } prefix = prefix+ch; i = i+1; }
    normalized :: String := ""; i = 0;
    while i < full.size() { ch :: String := full[i]; if ch == "\\" { ch = "/"; } normalized = normalized+ch; i = i+1; }
    while prefix.size() > 1 && prefix[prefix.size()-1] == "/" { prefix = vraylib.text_subtext(prefix,0,prefix.size()-1); }
    if normalized.size() <= prefix.size() { return normalized; }
    if vraylib.text_subtext(normalized,0,prefix.size()) != prefix || normalized[prefix.size()] != "/" { return normalized; }
    return vraylib.text_subtext(normalized,prefix.size()+1,normalized.size()-prefix.size()-1);
}
fn se_asset_breadcrumb(directory :: Array<Int64>,root :: String,path :: String,rectangle :: vraylib.Rectangle,editable :: Bool) -> Bool {
    x :: Float64 := rectangle.x;
    if se_button("Root",se_rect(x,rectangle.y,44.0,rectangle.height),editable) { se_text_write(directory,root); }
    x = x+48.0;
    relative :: String := se_ui_asset_relative(root,path);
    if path == root { return true; }
    if vraylib.is_path_absolute(relative) {
        label :: String := vfs.basename(vfs.dirname(path));
        if se_button(label,se_rect(x,rectangle.y,vmath.max(1.0,rectangle.width-48.0),rectangle.height),editable) { se_text_write(directory,vfs.dirname(path)); }
        return true;
    }
    prefix :: String := root; start :: Int64 := 0; i :: Int64 := 0;
    while i <= relative.size() {
        separator :: Bool := i == relative.size(); if !separator { separator = relative[i] == "/" || relative[i] == "\\"; }
        if separator && i > start {
            part :: String := vraylib.text_subtext(relative,start,i-start); prefix = vfs.join(prefix,part);
            width :: Float64 := vmath.min(100.0,se_measure(part,13)+12.0);
            if x+width <= rectangle.x+rectangle.width { if se_button(part,se_rect(x,rectangle.y,width,rectangle.height),editable) { se_text_write(directory,prefix); } x = x+width+4.0; }
            else { if x+34.0 <= rectangle.x+rectangle.width { if se_button("...",se_rect(x,rectangle.y,30.0,rectangle.height),editable) { se_text_write(directory,vfs.dirname(path)); } } return true; }
            start = i+1;
        }
        i = i+1;
    }
    return true;
}
fn se_output(app :: vraylib.App,logs :: Array,log_state :: Array<Int64>,rectangle :: vraylib.Rectangle) -> Bool {
    se_panel(rectangle,"Output");
    clip :: vraylib.Clip := app.clip(rectangle); se_clip_start(clip,rectangle);
    i :: Int64 := 0;
    while i < 6 && i < log_state[0] {
        index :: Int64 := (log_state[0]-1-i)%32;
        se_draw_text(se_fit(se_text_read(logs[index]),rectangle.width-16.0,13),int64(rectangle.x+8.0),int64(rectangle.y+34.0+float64(i)*23.0),13,vraylib.LIGHTGRAY);
        i = i + 1;
    }
    se_clip_finish(clip); return true;
}
fn se_commit_edit(ui :: SEUI,scene :: SEScene,staging :: SEScene,history :: SEHistory,bank :: SEAssets,directory :: Array<Int64>,scene_path :: Array<Int64>) -> Bool {
    field :: Int64 := ui.state[1];
    if field == -1 { return false; }
    text :: String := se_text_read(ui.text);
    if field == 18 || field == 19 {
        if field == 18 { se_text_copy(ui.tree_search,ui.text); ui.state[7] = 0; }
        else { se_text_copy(ui.asset_search,ui.text); ui.state[8] = 0; }
        tab :: Int64 := ui.state[30]; ui.state[1] = -1; ui.state[30] = 0;
        if tab != 0 && field == 19 { se_edit_begin(ui,17,-1,se_text_read(directory)); }
        return true;
    }
    if field == 16 {
        if ui.state[13] == 1 { se_load(scene,staging,text); se_history_reset(history,scene,true); ui.state[0] = -1; }
        else {
            if ui.state[13] == 3 {
                @bounded<16777216> region import_map {
                    imported :: VMGEMap := vmge_map_load(text,128,false);
                    se_import_map(staging,imported);
                };
                se_replace(scene,staging); se_record(history,scene); ui.state[0] = -1;
            } else { se_save(scene,text); history.saved(); }
        }
        if ui.state[13] != 3 { se_text_write(scene_path,text); } ui.state[13] = 0;
    } else {
        if field == 17 { if !vfs.is_dir(text) { throw "Asset directory not found"; } absolute :: String := text; if !vraylib.is_path_absolute(absolute) { absolute = vfs.join(vraylib.get_working_directory(),absolute); } se_text_write(directory,absolute); ui.state[8] = 0; ui.state[32] = 1; }
        else {
            id :: Int64 := ui.state[14]; slot :: Int64 := scene.require(id);
            if field == 14 { se_text_copy(scene.names[scene.offset+slot],ui.text); }
            if field == 15 { if scene.kind(id) == 1 && text != "" { se_asset(bank,text); } se_text_copy(scene.paths[scene.offset+slot],ui.text); }
            if field < 14 { scene.property(id,field,vmge_number(text)); }
            if field >= 64 && field < 80 { se_extra_set(scene,id,field-64,vmge_number(text)); }
            se_record(history,scene);
        }
    }
    tab :: Int64 := ui.state[30]; ui.state[1] = -1; ui.state[30] = 0;
    if tab != 0 {
        if field == 17 { se_edit_begin(ui,19,-1,se_text_read(ui.asset_search)); }
        else { if field < 16 && scene.valid(ui.state[14]) { se_inspector_tab(ui,scene,field,tab); } }
    }
    return true;
}
fn se_inspector_tab(ui :: SEUI,scene :: SEScene,field :: Int64,direction :: Int64) -> Bool {
    id :: Int64 := ui.state[14]; kind :: Int64 := scene.kind(id);
    next :: Int64 := field; tries :: Int64 := 0;
    while tries < 16 {
        next = (next+direction+16)%16;
        if next < 9 || next == 14 || (next >= 9 && next <= 12 && (kind == 1 || kind == 6 || kind == 7)) || (next == 13 && kind == 2) || (next == 15 && (kind == 1 || kind == 2)) {
            value :: String := ""; if next == 14 { value = scene.name(id); } else { if next == 15 { value = scene.path(id); } else { value = string(scene.number(id,next)); } }
            se_edit_begin(ui,next,id,value);
            # Bring the newly focused numeric group into view on short windows.
            if next < 14 {
                offset :: Float64 := 104.0; if kind == 1 { offset = offset+27.0; }
                if next < 9 { offset = offset+float64(next/3)*105.0+float64(next%3)*27.0; }
                else { offset = offset+315.0; if next < 13 { offset = offset+float64(next-9)*27.0; } }
                ui.state[24] = int64(vmath.max(0.0,vmath.floor((offset-60.0)/27.0)));
            } else { ui.state[24] = 0; }
            return true;
        }
        tries = tries+1;
    }
    return false;
}
fn se_ui_prepare(ui :: SEUI,directory :: Array) -> Bool { path :: String := se_text_read(directory); return se_asset_listing(ui,path); }
fn se_file_dialog(ui :: SEUI,rectangle :: vraylib.Rectangle) -> Bool {
    if ui.state[13] == 0 { return false; }
    se_panel(rectangle,"Scene file path - Enter applies, Escape cancels");
    se_field("Path",se_text_read(ui.text),se_rect(rectangle.x+8.0,rectangle.y+35.0,rectangle.width-16.0,28.0),true,true);
    return se_button("Apply",se_rect(rectangle.x+8.0,rectangle.y+75.0,80.0,28.0),true);
}
fn se_extra_label(kind :: Int64,field :: Int64) -> String {
    if kind != 5 { if field == 0 { return "Intensity"; } if field == 1 { return "Radius"; } return "Ambient"; }
    if field == 0 { return "Zenith R"; } if field == 1 { return "Zenith G"; } if field == 2 { return "Zenith B"; }
    if field == 3 { return "Horizon R"; } if field == 4 { return "Horizon G"; } if field == 5 { return "Horizon B"; }
    if field == 6 { return "Brightness"; } if field == 7 { return "Sun degrees"; } if field == 8 { return "Sun energy"; }
    if field == 9 { return "Sun slot/-1"; } return "Ambient";
}
fn se_node_picker_open(ui :: SEUI) -> Bool {
    ui.state[47] = 1; ui.state[48] = 0; ui.state[49] = 0; ui.state[50] = 0; ui.state[51] = ui.state[0];
    se_edit_begin(ui,20,-1,""); return true;
}
fn se_picker_match(kind :: Int64,query :: String) -> Bool {
    if kind < 0 || kind >= 9 { return false; }
    if kind == 8 { return query == "" || se_contains("Daylight Scene preset sky sun environment",query); } return se_node_matches(kind,query);
}
fn se_node_picker_input(ui :: SEUI) -> Bool {
    if ui.state[47] == 0 { return false; }
    se_edit_input(ui);
    if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) { ui.state[47] = 0; ui.state[1] = -1; return true; }
    query :: String := se_text_read(ui.text); kind :: Int64 := ui.state[48];
    if !se_picker_match(kind,query) { kind = 0; while kind < 9 && !se_picker_match(kind,query) { kind = kind+1; } }
    direction :: Int64 := 0; if se_key(vraylib.KEY_DOWN) { direction = 1; } if se_key(vraylib.KEY_UP) { direction = -1; }
    if direction != 0 { candidate :: Int64 := kind+direction; while candidate >= 0 && candidate < 9 { if se_picker_match(candidate,query) { kind = candidate; break; } candidate = candidate+direction; } }
    ui.state[48] = kind;
    if vraylib.is_key_pressed(vraylib.KEY_ENTER) && kind < 9 { ui.state[50] = 1; }
    ui.state[30] = 0; return true;
}
fn se_create_catalog_node(scene :: SEScene,ui :: SEUI,camera :: vraylib.Camera,cache :: SETransforms) -> Int64 {
    kind :: Int64 := ui.state[48]; parent :: Int64 := -1;
    if ui.state[49] == 0 && scene.valid(ui.state[51]) { parent = ui.state[51]; }
    if kind == 8 {
        se_catalog_room(scene,2);
        sky :: Int64 := scene.spawn(5,"Sky",parent); sun :: Int64 := scene.spawn(6,"Sun",parent);
        scene.property(sun,3,55.0); scene.property(sun,4,-35.0); se_extra_set(scene,sky,9,float64(sun%128)); se_make_active(scene,sky); return sky;
    }
    se_catalog_room(scene,1);
    position :: vraylib.Vector3 := vraylib.vec3(ui.numbers[6],vmath.max(0.5,ui.numbers[7]),ui.numbers[8]);
    pitch :: Float64 := 0.0; yaw :: Float64 := 0.0; fov :: Float64 := 70.0;
    if kind == 1 || kind == 7 {
        if parent != -1 { position = vraylib.math_vector3_transform(position,vraylib.math_matrix_invert(se_world(scene,cache,parent))); }
    }
    if kind == 2 {
        view :: vraylib.Camera3D := camera.view(); eye :: vraylib.Vector3 := view.position; target :: vraylib.Vector3 := view.target;
        if parent != -1 { inverse :: vraylib.Matrix := vraylib.math_matrix_invert(se_world(scene,cache,parent)); eye = vraylib.math_vector3_transform(eye,inverse); target = vraylib.math_vector3_transform(target,inverse); }
        forward :: vraylib.Vector3 := vraylib.direction(eye,target);
        position = eye; pitch = -vmath.asin(vmath.clamp(forward.y,-1.0,1.0))*57.295779513; yaw = vmath.atan2(forward.x,forward.z)*57.295779513;
        fov = view.fovy; if view.projection != vraylib.CAMERA_PERSPECTIVE { fov = ui.view_values[0]; } fov = vmath.clamp(fov,1.0,179.0);
        if !(vmath.abs(pitch) <= 360.0 && vmath.abs(yaw) <= 360.0 && fov >= 1.0 && fov <= 179.0) { throw "Invalid camera creation view"; }
    }
    if kind == 1 || kind == 2 || kind == 7 { if !(vmath.abs(position.x) <= 1000000.0 && vmath.abs(position.y) <= 1000000.0 && vmath.abs(position.z) <= 1000000.0) { throw "Node creation position exceeds scene bounds"; } }
    # All fallible placement validation precedes reserving a scene slot.
    id :: Int64 := scene.spawn(kind,se_type_name(kind),parent);
    if kind == 1 || kind == 2 || kind == 7 { scene.property(id,0,position.x); scene.property(id,1,position.y); scene.property(id,2,position.z); }
    if kind == 6 { scene.property(id,3,55.0); scene.property(id,4,-35.0); }
    if kind == 2 { scene.property(id,3,pitch); scene.property(id,4,yaw); scene.property(id,13,fov); }
    return id;
}
fn se_node_picker_draw(ui :: SEUI,scene :: SEScene,bounds :: vraylib.Rectangle) -> Bool {
    if ui.state[47] == 0 { return false; }
    vraylib.draw_rectangle_rec(bounds,vraylib.rgba(0,0,0,170));
    width :: Float64 := vmath.min(560.0,bounds.width-24.0); height :: Float64 := vmath.min(570.0,bounds.height-24.0);
    box :: vraylib.Rectangle := se_rect((bounds.width-width)*0.5,(bounds.height-height)*0.5,width,height);
    se_panel(box,"Add Node / arrows select, Enter creates, Escape cancels");
    se_field("Search",se_text_read(ui.text),se_rect(box.x+10.0,box.y+34.0,width-20.0,28.0),true,true);
    visible_rows :: Int64 := int64(vmath.max(1.0,vmath.floor((height-152.0)/40.0))); selected_row :: Int64 := 0; matches :: Int64 := 0; kind :: Int64 := 0;
    while kind < 9 { if se_picker_match(kind,se_text_read(ui.text)) { if kind == ui.state[48] { selected_row = matches; } matches = matches+1; } kind = kind+1; }
    first :: Int64 := int64(vmath.max(0.0,float64(selected_row-visible_rows+1))); row :: Int64 := 0; kind = 0;
    while kind < 9 {
        if se_picker_match(kind,se_text_read(ui.text)) {
            item :: vraylib.Rectangle := se_rect(box.x+10.0,box.y+70.0+float64(row-first)*40.0,width-20.0,37.0);
            if row >= first && row < first+visible_rows {
                title :: String := "Daylight Scene"; description :: String := "Preset / add linked Sky and Sun in one undo step";
                if kind < 8 { title = se_type_name(kind); description = se_node_category(kind)+" / "+se_node_description(kind); }
                if ui.state[48] == kind { vraylib.draw_rectangle_rec(item,vraylib.rgba(47,76,110,255)); }
                se_draw_heading(title,int64(item.x+8.0),int64(item.y+3.0),14,vraylib.SKYBLUE);
                se_draw_text(se_fit(description,width-40.0,12),int64(item.x+8.0),int64(item.y+21.0),12,vraylib.LIGHTGRAY);
                if se_hover(item) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) { ui.state[48] = kind; }
            } row = row+1;
        } kind = kind+1;
    }
    parent :: String := "Parent: root"; if ui.state[49] == 0 && scene.valid(ui.state[51]) { parent = "Parent: "+scene.name(ui.state[51]); }
    if se_button(parent,se_rect(box.x+10.0,box.y+height-71.0,width-20.0,25.0),true) { ui.state[49] = 1-ui.state[49]; }
    if se_button("Cancel",se_rect(box.x+10.0,box.y+height-38.0,90.0,28.0),true) { ui.state[47] = 0; ui.state[1] = -1; ui.state[39] = 1; }
    needed :: Int64 := 1; if ui.state[48] == 8 { needed = 2; }
    se_draw_text(string(scene.count())+" / 128 nodes",int64(box.x+110.0),int64(box.y+height-30.0),13,vraylib.GRAY);
    if se_button("Create",se_rect(box.x+width-100.0,box.y+height-38.0,90.0,28.0),ui.state[48] < 9 && scene.count()+needed <= 128) { ui.state[50] = 1; }
    return true;
}
fn se_catalog_room(scene :: SEScene,needed :: Int64) -> Bool {
    found :: Int64 := 0; slot :: Int64 := 0; while slot < 128 && found < needed {
        if scene.meta[slot*8] == 0 {
            if scene.clocks[slot] >= 72057594037927935 || scene.meta[slot*8+1] >= 72057594037927935 { throw "Scene generation exhausted"; }
            found = found+1;
        } slot = slot+1;
    } if found < needed { throw "Node creation capacity reached (128 slots)"; } return true;
}
