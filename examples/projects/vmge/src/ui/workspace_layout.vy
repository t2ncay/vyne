use "widgets.vy";

# Persisted values are logical units, never framebuffer pixels.
# state: workspace, source tab, inspector tab, drawer tab, maximized, drag, dirty.
# values: source fraction, inspector width, drawer height, DPI.
interface SSLayout { state :: Array<Int64>, values :: Array<Float64>, }
fn ss_layout() -> SSLayout { return SSLayout([2,0,0,0,0,0,0],[0.59,280.0,164.0,1.0]); }
fn ss_layout_validate(layout :: SSLayout) -> Bool {
    i :: Int64 := 0;
    while i < 3 { if layout.state[i] < 0 || layout.state[i] > 3 { return false; } i = i+1; }
    if layout.state[3] < 0 || layout.state[3] > 4 { return false; }
    if layout.state[4] < 0 || layout.state[4] > 1 { return false; }
    if !(layout.values[0] >= 0.2 && layout.values[0] <= 0.8) { return false; }
    if !(layout.values[1] >= 180.0 && layout.values[1] <= 600.0) { return false; }
    if !(layout.values[2] >= 0.0 && layout.values[2] <= 480.0) { return false; }
    return layout.values[3] >= 0.75 && layout.values[3] <= 3.0;
}
fn ss_layout_encode(layout :: SSLayout) -> String {
    return "VMGE_LAYOUT|1\n"+string(layout.state[0])+"|"+string(layout.state[1])+"|"+string(layout.state[2])+"|"+string(layout.state[3])+"|"+string(layout.state[4])+"\n"+string(layout.values[0])+"|"+string(layout.values[1])+"|"+string(layout.values[2])+"|"+string(layout.values[3])+"\n";
}
fn ss_layout_split(text :: String,separator :: String) -> Array {
    parts :: Array := []; builder :: Int64 := vcore.sb_create(); i :: Int64 := 0;
    while i < text.size() { if text[i] == separator { parts.push(vcore.sb_build(builder)); builder = vcore.sb_create(); } else { vcore.sb_append(builder,text[i]); } i = i+1; }
    parts.push(vcore.sb_build(builder)); return parts;
}
fn ss_layout_number(text :: String) -> Float64 {
    if text.size() == 0 { throw "Empty layout number"; } i :: Int64 := 0; dots :: Int64 := 0; digits :: Int64 := 0;
    while i < text.size() { ch :: String := text[i]; if ch == "." { dots = dots+1; if dots > 1 { throw "Invalid layout number"; } } else { if ch != "0" && ch != "1" && ch != "2" && ch != "3" && ch != "4" && ch != "5" && ch != "6" && ch != "7" && ch != "8" && ch != "9" { throw "Invalid layout digit"; } digits = digits+1; } i = i+1; }
    if digits == 0 { throw "Invalid layout number"; } value :: Float64 := float64(text); if !(value >= 0.0 && value <= 1000000.0) { throw "Layout value out of range"; } return value;
}
fn ss_layout_decode(layout :: SSLayout,text :: String) -> Bool {
    rows :: Array := ss_layout_split(text,"\n"); if rows.size() < 3 || rows[0] != "VMGE_LAYOUT|1" { return false; }
    tabs :: Array := ss_layout_split(rows[1],"|"); sizes :: Array := ss_layout_split(rows[2],"|"); if tabs.size() != 5 || sizes.size() != 4 { return false; }
    candidate :: SSLayout := ss_layout();
    try {
        i :: Int64 := 0; while i < 5 { value :: Float64 := ss_layout_number(tabs[i]); if value != vmath.floor(value) { return false; } candidate.state[i] = int64(value); i = i+1; }
        i = 0; while i < 4 { candidate.values[i] = ss_layout_number(sizes[i]); i = i+1; }
    } catch (error) { return false; }
    if !ss_layout_validate(candidate) { return false; }
    i :: Int64 := 0; while i < 5 { layout.state[i] = candidate.state[i]; i = i+1; }
    i = 0; while i < 4 { layout.values[i] = candidate.values[i]; i = i+1; }
    layout.state[6] = 0; return true;
}
fn ss_layout_load(layout :: SSLayout,path :: String) -> Bool {
    if !vfs.exists(path) { return false; } return ss_layout_decode(layout,vfs.read(path));
}
fn ss_layout_save(layout :: SSLayout,path :: String) -> Bool {
    layout.state[6] = 1;
    if !ss_layout_validate(layout) { return false; }
    if !vfs.write_atomic(path,ss_layout_encode(layout)) { return false; } layout.state[6] = 0; return true;
}
# Regions: navigation, actions, source, preview, inspector, diagnostics, status,
# then source / inspector / drawer splitters (7..9).
fn ss_layout_region(layout :: SSLayout,bounds :: vraylib.Rectangle,part :: Int64) -> vraylib.Rectangle {
    s :: Float64 := layout.values[3]; nav :: Float64 := 38.0*s; bar :: Float64 := 40.0*s; status :: Float64 := 24.0*s; gap :: Float64 := 5.0*s;
    nav = vmath.min(nav,bounds.height); bar = vmath.min(bar,vmath.max(0.0,bounds.height-nav)); status = vmath.min(status,vmath.max(0.0,bounds.height-nav-bar));
    y :: Float64 := bounds.y+nav+bar; h :: Float64 := vmath.max(0.0,bounds.height-nav-bar-status);
    gap = vmath.min(gap,h);
    drawer :: Float64 := vmath.clamp(layout.values[2]*s,0.0,vmath.max(0.0,h*0.45));
    inspector :: Float64 := vmath.min(layout.values[1]*s,vmath.max(0.0,bounds.width*0.28));
    work :: Float64 := vmath.max(0.0,bounds.width-inspector-gap);
    source :: Float64 := vmath.clamp(work*layout.values[0],0.0,work);
    if part == 0 { return se_rect(bounds.x,bounds.y,bounds.width,nav); }
    if part == 1 { return se_rect(bounds.x,bounds.y+nav,bounds.width,bar); }
    if part == 6 { return se_rect(bounds.x,bounds.y+bounds.height-status,bounds.width,status); }
    if layout.state[4] != 0 {
        if part == 3 { return se_rect(bounds.x,y,bounds.width,h); }
        return se_rect(bounds.x,y,0.0,0.0);
    }
    if part == 2 { return se_rect(bounds.x,y,vmath.max(0.0,source-gap),vmath.max(0.0,h-drawer-gap)); }
    if part == 3 { return se_rect(bounds.x+source,y,vmath.max(0.0,work-source),vmath.max(0.0,h-drawer-gap)); }
    if part == 4 { return se_rect(bounds.x+work+gap,y,inspector,vmath.max(0.0,h-drawer-gap)); }
    if part == 5 { return se_rect(bounds.x,y+h-drawer,bounds.width,drawer); }
    if part == 7 { return se_rect(bounds.x+source-gap,y,gap,vmath.max(0.0,h-drawer-gap)); }
    if part == 8 { return se_rect(bounds.x+work,y,gap,vmath.max(0.0,h-drawer-gap)); }
    return se_rect(bounds.x,y+h-drawer-gap,bounds.width,gap);
}
fn ss_layout_update(layout :: SSLayout,bounds :: vraylib.Rectangle) -> Bool {
    ss_theme_dpi(layout.values[3]);
    if vraylib.is_key_pressed(vraylib.KEY_F11) { layout.state[4] = 1-layout.state[4]; layout.state[6] = 1; }
    if layout.state[4] != 0 { return true; }
    if vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) {
        i :: Int64 := 7; while i <= 9 { if se_hover(ss_layout_region(layout,bounds,i)) { layout.state[5] = i; } i = i+1; }
    }
    if !vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) { layout.state[5] = 0; return true; }
    mouse :: vraylib.Vector2 := vraylib.get_mouse_position(); s :: Float64 := layout.values[3];
    if layout.state[5] == 7 { work :: Float64 := bounds.width-vmath.min(layout.values[1]*s,bounds.width*0.28)-5.0*s; if work > 0.0 { layout.values[0] = vmath.clamp((mouse.x-bounds.x)/work,0.2,0.8); } }
    if layout.state[5] == 8 { layout.values[1] = vmath.clamp((bounds.x+bounds.width-mouse.x)/s,180.0,600.0); }
    if layout.state[5] == 9 { layout.values[2] = vmath.clamp((bounds.y+bounds.height-24.0*s-mouse.y)/s,0.0,480.0); }
    if layout.state[5] != 0 { layout.state[6] = 1; } return true;
}
fn ss_tab(label :: String,box :: vraylib.Rectangle,selected :: Bool) -> Bool {
    vraylib.draw_rectangle_rec(box,ss_color(1));
    if se_hover(box) { vraylib.draw_rectangle_rec(box,ss_color(2)); }
    se_draw_text(se_fit(label,box.width-ss_scale(16.0),ss_font_size()),int64(box.x+ss_scale(8.0)),int64(box.y+ss_scale(8.0)),ss_font_size(),ss_color(5));
    if selected { vraylib.draw_rectangle_rec(se_rect(box.x,box.y+box.height-ss_scale(2.0),box.width,ss_scale(2.0)),ss_color(6)); }
    return ss_theme_input[0] != 0 && se_hover(box) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT);
}
fn ss_workspace_navigation(layout :: SSLayout,bounds :: vraylib.Rectangle) -> Int64 {
    box :: vraylib.Rectangle := ss_layout_region(layout,bounds,0); vraylib.draw_rectangle_rec(box,ss_color(1));
    se_draw_heading("VMGE",int64(box.x+ss_scale(12.0)),int64(box.y+ss_scale(9.0)),ss_font_size(),ss_color(4));
    labels := ["Scene","World","Shaders","Rendering"]; i :: Int64 := 0;
    tab_width :: Float64 := vmath.min(ss_scale(104.0),vmath.max(0.0,(box.width-ss_scale(84.0))/4.0));
    while i < 4 { if ss_tab(labels[i],se_rect(box.x+ss_scale(84.0)+float64(i)*tab_width,box.y,tab_width,box.height),layout.state[0] == i) { layout.state[0] = i; layout.state[6] = 1; } i = i+1; }
    return layout.state[0];
}
