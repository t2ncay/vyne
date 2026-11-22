use "theme.vy";
use native vmath;

# UI and text storage are created before frames; focus and drag state are numeric.
interface SEUI {
    state :: Array<Int64>,
    numbers :: Array<Float64>,
    text :: Array<Int64>,
    edit_buffer :: Array<Int64>,
    asset_names :: Array<Int64>,
    asset_kinds :: Array<Int64>,
    asset_directory :: Array<Int64>,
    asset_search :: Array<Int64>,
    tree_search :: Array<Int64>,
    collapsed :: Array<Int64>,
    preview_tokens :: Array<Int64>,
    preview_paths :: Array<Int64>,
    browser_state :: Array<Int64>,
    external_path :: Array<Int64>,
    asset_notice :: Array<Int64>,
    view_state :: Array<Int64>,
    view_values :: Array<Float64>,
}
fn se_ui(text :: Array) -> SEUI {
    state :: Array<Int64> := []; state.reserve(64); state.resize(64,0);
    state[0] = -1; state[1] = -1; state[2] = -1; state[5] = -1; state[6] = -1; state[14] = -1;
    numbers :: Array<Float64> := []; numbers.reserve(32); numbers.resize(32,0.0);
    numbers[0] = 230.0; numbers[1] = 280.0; numbers[2] = 190.0;
    numbers[3] = 0.65; numbers[4] = 0.45; numbers[5] = 18.0; numbers[7] = 1.0; numbers[8] = 2.0;
    numbers[15] = 2.0; numbers[16] = 1.0;
    edit_buffer :: Array<Int64> := []; edit_buffer.reserve(256); edit_buffer.resize(256,0);
    names :: Array<Int64> := []; names.reserve(131072); names.resize(131072,0);
    kinds :: Array<Int64> := []; kinds.reserve(512); kinds.resize(512,0);
    directory :: Array<Int64> := []; directory.reserve(256); directory.resize(256,0);
    search :: Array<Int64> := []; search.reserve(256); search.resize(256,0);
    tree_search :: Array<Int64> := []; tree_search.reserve(256); tree_search.resize(256,0);
    collapsed :: Array<Int64> := []; collapsed.reserve(128); collapsed.resize(128,-1);
    preview_tokens :: Array<Int64> := []; preview_tokens.reserve(16); preview_tokens.resize(16,0);
    preview_paths :: Array<Int64> := []; preview_paths.reserve(4096); preview_paths.resize(4096,0);
    browser_state :: Array<Int64> := [0,0,0,-1];
    external_path :: Array<Int64> := []; external_path.reserve(256); external_path.resize(256,0);
    asset_notice :: Array<Int64> := []; asset_notice.reserve(256); asset_notice.resize(256,0);
    view_state :: Array<Int64> := [0]; view_values :: Array<Float64> := [70.0];
    state[28] = -1;
    return SEUI(state,numbers,text,edit_buffer,names,kinds,directory,search,tree_search,collapsed,preview_tokens,preview_paths,browser_state,external_path,asset_notice,view_state,view_values);
}
fn se_rect(x :: Float64,y :: Float64,width :: Float64,height :: Float64) -> vraylib.Rectangle { return vraylib.Rectangle(x,y,width,height); }
fn se_hover(rectangle :: vraylib.Rectangle) -> Bool {
    mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
    if se_theme_tokens[3] != 0 && !vraylib.check_collision_point_rec(mouse,se_rect(se_theme_metrics[2],se_theme_metrics[3],se_theme_metrics[4],se_theme_metrics[5])) { return false; }
    return vraylib.check_collision_point_rec(mouse,rectangle);
}
fn se_button(label :: String,rectangle :: vraylib.Rectangle,enabled :: Bool) -> Bool {
    hovered :: Bool := se_hover(rectangle);
    fill :: vraylib.Color := vraylib.rgba(43,49,62,255);
    if hovered && enabled { fill = vraylib.rgba(61,76,99,255); }
    if !enabled { fill = vraylib.rgba(30,35,44,255); }
    vraylib.draw_rectangle_rec(rectangle,fill);
    vraylib.draw_rectangle_lines_ex(rectangle,1.0,vraylib.rgba(62,70,87,255));
    selected :: Bool := (label == "T" && se_theme_tokens[4] == 0) || (label == "R" && se_theme_tokens[4] == 1) || (label == "S" && se_theme_tokens[4] == 2) || (label == "Snap" && se_theme_tokens[5] != 0) || (label == "Stop" && se_theme_tokens[6] != 0);
    if selected && enabled { vraylib.draw_rectangle_lines_ex(rectangle,2.0,vraylib.rgba(112,165,245,255)); vraylib.draw_rectangle_rec(se_rect(rectangle.x+2.0,rectangle.y+rectangle.height-3.0,rectangle.width-4.0,2.0),vraylib.SKYBLUE); }
    if hovered && enabled {
        if label == "T" { se_theme_tokens[7] = 1; } if label == "R" { se_theme_tokens[7] = 2; } if label == "S" { se_theme_tokens[7] = 3; }
        if label == "Snap" { se_theme_tokens[7] = 4; } if label == "Focus" { se_theme_tokens[7] = 5; } if label == "Play" || label == "Stop" { se_theme_tokens[7] = 6; }
        if label == "Assign to mesh" { se_theme_tokens[7] = 7; } if label == "New mesh node" { se_theme_tokens[7] = 8; }
    }
    tint :: vraylib.Color := vraylib.rgba(226,232,241,255); if !enabled { tint = vraylib.rgba(105,116,135,255); }
    size :: Int64 := 14;
    while size > 12 && se_measure(label,size) > rectangle.width-12.0 { size = size-1; }
    se_draw_text(se_fit(label,rectangle.width-12.0,size),int64(rectangle.x+6.0),int64(rectangle.y+(rectangle.height-float64(size))*0.5),size,tint);
    return hovered && enabled && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT);
}
fn se_panel(rectangle :: vraylib.Rectangle,title :: String) -> Bool {
    vraylib.draw_rectangle_rec(rectangle,vraylib.rgba(25,30,39,255));
    vraylib.draw_rectangle_rec(se_rect(rectangle.x,rectangle.y,rectangle.width,26.0),vraylib.rgba(32,38,49,255));
    vraylib.draw_rectangle_lines_ex(rectangle,1.0,vraylib.rgba(45,54,69,255));
    se_draw_heading(se_fit(title,rectangle.width-16.0,14),int64(rectangle.x+8.0),int64(rectangle.y+6.0),14,vraylib.rgba(191,211,239,255));
    return true;
}
fn se_edit_begin(ui :: SEUI,field :: Int64,node :: Int64,text :: String) -> Bool {
    se_text_write(ui.text,text); ui.state[1] = field; ui.state[14] = node;
    ui.state[26] = ui.text[0]; ui.state[27] = 0; ui.state[30] = 0;
    se_edit_metrics(ui);
    return true;
}
fn se_edit_input(ui :: SEUI) -> Bool {
    if ui.state[1] == -1 { return false; }
    control :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL) || vraylib.is_key_down(vraylib.KEY_RIGHT_CONTROL);
    shift :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT) || vraylib.is_key_down(vraylib.KEY_RIGHT_SHIFT);
    if control && vraylib.is_key_pressed(vraylib.KEY_A) { ui.state[27] = 0; ui.state[26] = ui.text[0]; }
    if control && (vraylib.is_key_pressed(vraylib.KEY_C) || vraylib.is_key_pressed(vraylib.KEY_X)) {
        left :: Int64 := int64(vmath.min(float64(ui.state[26]),float64(ui.state[27])));
        right :: Int64 := int64(vmath.max(float64(ui.state[26]),float64(ui.state[27])));
        if right > left { vraylib.set_clipboard_text(se_edit_slice(ui.text,left,right)); if vraylib.is_key_pressed(vraylib.KEY_X) { se_edit_delete(ui,left,right); } }
    }
    if control && vraylib.is_key_pressed(vraylib.KEY_V) {
        paste :: String := vraylib.get_clipboard_text();
        # Validate capacity before modifying the selection, including UTF-8 bytes.
        if paste.size() < ui.edit_buffer.size() {
            se_text_write(ui.edit_buffer,paste); se_edit_insert(ui,ui.edit_buffer);
        }
    }
    if se_key(vraylib.KEY_LEFT) || se_key(vraylib.KEY_RIGHT) || se_key(vraylib.KEY_HOME) || se_key(vraylib.KEY_END) {
        caret :: Int64 := ui.state[26];
        if se_key(vraylib.KEY_LEFT) { if !shift && ui.state[26] != ui.state[27] { caret = int64(vmath.min(float64(ui.state[26]),float64(ui.state[27]))); } else { caret = se_edit_previous(ui.text,caret); } }
        if se_key(vraylib.KEY_RIGHT) { if !shift && ui.state[26] != ui.state[27] { caret = int64(vmath.max(float64(ui.state[26]),float64(ui.state[27]))); } else { caret = se_edit_next(ui.text,caret); } }
        if se_key(vraylib.KEY_HOME) { caret = 0; }
        if se_key(vraylib.KEY_END) { caret = ui.text[0]; }
        ui.state[26] = caret; if !shift { ui.state[27] = caret; }
    }
    code :: Int64 := vraylib.get_char_pressed();
    while code > 0 {
        if code >= 32 && code != 127 && !control { ui.edit_buffer[0] = se_utf8_put(ui.edit_buffer,1,code); se_edit_insert(ui,ui.edit_buffer); }
        code = vraylib.get_char_pressed();
    }
    if se_key(vraylib.KEY_BACKSPACE) || se_key(vraylib.KEY_DELETE) {
        left :: Int64 := int64(vmath.min(float64(ui.state[26]),float64(ui.state[27])));
        right :: Int64 := int64(vmath.max(float64(ui.state[26]),float64(ui.state[27])));
        if left == right { if se_key(vraylib.KEY_BACKSPACE) { left = se_edit_previous(ui.text,left); } else { right = se_edit_next(ui.text,right); } }
        se_edit_delete(ui,left,right);
    }
    se_edit_metrics(ui);
    if ui.state[1] == 18 { if se_text_read(ui.tree_search) != se_text_read(ui.text) { se_text_copy(ui.tree_search,ui.text); ui.state[7] = 0; } }
    if ui.state[1] == 19 { if se_text_read(ui.asset_search) != se_text_read(ui.text) { se_text_copy(ui.asset_search,ui.text); ui.state[8] = 0; } }
    if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) { ui.state[1] = -1; ui.state[13] = 0; }
    if vraylib.is_key_pressed(vraylib.KEY_TAB) { ui.state[30] = 1; if shift { ui.state[30] = -1; } return true; }
    return vraylib.is_key_pressed(vraylib.KEY_ENTER);
}
fn se_field(label :: String,value :: String,rectangle :: vraylib.Rectangle,focused :: Bool,enabled :: Bool) -> Bool {
    fill :: vraylib.Color := vraylib.rgba(40,45,55,255);
    if focused { fill = vraylib.rgba(50,75,108,255); }
    vraylib.draw_rectangle_rec(rectangle,fill);
    edge :: vraylib.Color := vraylib.rgba(57,65,81,255); if focused { edge = vraylib.rgba(110,164,245,255); }
    vraylib.draw_rectangle_lines_ex(rectangle,1.0,edge);
    label_width :: Float64 := vmath.min(84.0,rectangle.width*0.38);
    se_draw_text(se_fit(label,label_width-8.0,13),int64(rectangle.x+5.0),int64(rectangle.y+5.0),13,vraylib.rgba(147,161,184,255));
    x :: Float64 := rectangle.x+label_width; available :: Float64 := rectangle.width-label_width-8.0;
    if focused && se_theme_metrics[0] > available { x = x-(se_theme_metrics[0]-available); }
    # Raw scissor temporarily narrows the one managed panel clip, then restores it.
    left :: Float64 := rectangle.x+label_width; top :: Float64 := rectangle.y+2.0;
    right :: Float64 := rectangle.x+rectangle.width-4.0; bottom :: Float64 := rectangle.y+rectangle.height-2.0;
    if se_theme_tokens[3] != 0 { left = vmath.max(left,se_theme_metrics[2]); top = vmath.max(top,se_theme_metrics[3]); right = vmath.min(right,se_theme_metrics[2]+se_theme_metrics[4]); bottom = vmath.min(bottom,se_theme_metrics[3]+se_theme_metrics[5]); }
    vraylib.begin_scissor_mode(int64(left),int64(top),int64(vmath.max(0.0,right-left)),int64(vmath.max(0.0,bottom-top)));
    try { se_field_text(value,x,rectangle,focused,enabled); } finally {
        if se_theme_tokens[3] != 0 { vraylib.begin_scissor_mode(int64(se_theme_metrics[2]),int64(se_theme_metrics[3]),int64(se_theme_metrics[4]),int64(se_theme_metrics[5])); }
        else { vraylib.end_scissor_mode(); }
    }
    return enabled && se_hover(rectangle) && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT);
}
fn se_field_text(value :: String,x :: Float64,rectangle :: vraylib.Rectangle,focused :: Bool,enabled :: Bool) -> Bool {
    if focused {
        left :: Float64 := vmath.min(se_theme_metrics[0],se_theme_metrics[1]); right :: Float64 := vmath.max(se_theme_metrics[0],se_theme_metrics[1]);
        vraylib.draw_rectangle_rec(se_rect(x+left,rectangle.y+3.0,right-left,rectangle.height-6.0),vraylib.rgba(69,108,167,255));
    }
    tint :: vraylib.Color := vraylib.WHITE; if !enabled && !focused { tint = vraylib.GRAY; }
    se_draw_text(value,int64(x),int64(rectangle.y+5.0),14,tint);
    if focused && int64(vraylib.get_time()*2.0)%2 == 0 { vraylib.draw_rectangle_rec(se_rect(x+se_theme_metrics[0],rectangle.y+4.0,1.0,rectangle.height-8.0),vraylib.WHITE); }
    return true;
}
fn se_key(key :: Int64) -> Bool { return vraylib.is_key_pressed(key) || vraylib.is_key_pressed_repeat(key); }
fn se_edit_previous(bytes :: Array<Int64>,caret :: Int64) -> Int64 {
    if caret <= 0 { return 0; } position :: Int64 := caret-1;
    while position > 0 && bytes[position+1] >= 128 && bytes[position+1] < 192 { position = position-1; }
    return position;
}
fn se_edit_next(bytes :: Array<Int64>,caret :: Int64) -> Int64 {
    if caret >= bytes[0] { return bytes[0]; } position :: Int64 := caret+1;
    while position < bytes[0] && bytes[position+1] >= 128 && bytes[position+1] < 192 { position = position+1; }
    return position;
}
fn se_edit_slice(bytes :: Array<Int64>,left :: Int64,right :: Int64) -> String {
    builder :: Int64 := vcore.sb_create(); i :: Int64 := left+1;
    while i <= right { vcore.sb_append_char(builder,bytes[i]); i = i+1; } return vcore.sb_build(builder);
}
fn se_edit_delete(ui :: SEUI,left :: Int64,right :: Int64) -> Bool {
    i :: Int64 := right+1;
    while i <= ui.text[0] { ui.text[i-right+left] = ui.text[i]; i = i+1; }
    ui.text[0] = ui.text[0]-(right-left); ui.state[26] = left; ui.state[27] = left; return true;
}
fn se_edit_insert(ui :: SEUI,source :: Array<Int64>) -> Bool {
    left :: Int64 := int64(vmath.min(float64(ui.state[26]),float64(ui.state[27])));
    right :: Int64 := int64(vmath.max(float64(ui.state[26]),float64(ui.state[27])));
    if ui.text[0]-(right-left)+source[0] >= ui.text.size() { return false; }
    # Single-line editors ignore control characters in clipboard text.
    i :: Int64 := 1; while i <= source[0] { if source[i] < 32 || source[i] == 127 { return false; } i = i+1; }
    se_edit_delete(ui,left,right); i = ui.text[0];
    while i > left { ui.text[i+source[0]] = ui.text[i]; i = i-1; }
    i = 1; while i <= source[0] { ui.text[left+i] = source[i]; i = i+1; }
    ui.text[0] = ui.text[0]+source[0]; ui.state[26] = left+source[0]; ui.state[27] = ui.state[26]; return true;
}
fn se_edit_metrics(ui :: SEUI) -> Bool {
    se_theme_metrics[0] = se_measure(se_edit_slice(ui.text,0,ui.state[26]),14);
    se_theme_metrics[1] = se_measure(se_edit_slice(ui.text,0,ui.state[27]),14); return true;
}
fn se_tool_button(label :: String,rectangle :: vraylib.Rectangle,enabled :: Bool,selected :: Bool,tip :: String) -> Bool {
    pressed :: Bool := se_button(label,rectangle,enabled);
    if selected { vraylib.draw_rectangle_lines_ex(rectangle,2.0,vraylib.rgba(115,170,250,255)); vraylib.draw_rectangle_rec(se_rect(rectangle.x+2.0,rectangle.y+rectangle.height-3.0,rectangle.width-4.0,2.0),vraylib.SKYBLUE); }
    if enabled && se_hover(rectangle) {
        if tip == "Toggle node visibility" { se_theme_tokens[7] = 9; return pressed; }
        width :: Float64 := se_measure(tip,13)+16.0;
        x :: Float64 := vmath.clamp(rectangle.x,4.0,vmath.max(4.0,float64(vraylib.get_screen_width())-width-4.0));
        box :: vraylib.Rectangle := se_rect(x,rectangle.y+rectangle.height+6.0,width,26.0);
        vraylib.draw_rectangle_rec(box,vraylib.rgba(14,19,28,245)); se_draw_text(tip,int64(x+8.0),int64(box.y+6.0),13,vraylib.LIGHTGRAY);
    }
    return pressed;
}
fn se_viewport(ui :: SEUI,width :: Int64,height :: Int64) -> vraylib.Rectangle {
    se_ui_toolbar_state(ui);
    left :: Float64 := vmath.clamp(ui.numbers[0],150.0,vmath.min(400.0,float64(width)*0.3));
    right :: Float64 := vmath.clamp(ui.numbers[1],190.0,vmath.min(420.0,float64(width)*0.33));
    bottom :: Float64 := vmath.clamp(ui.numbers[2],100.0,vmath.max(100.0,float64(height)-200.0));
    return se_rect(left+4.0,46.0,vmath.max(64.0,float64(width)-left-right-8.0),vmath.max(64.0,float64(height)-bottom-50.0));
}
fn se_ui_toolbar_state(ui :: SEUI) -> Bool {
    se_theme_tokens[4] = ui.state[3]; se_theme_tokens[5] = int64(ui.numbers[16]); se_theme_tokens[6] = ui.state[4]; se_theme_tokens[7] = 0; return true;
}
fn se_ui_overlays(ui :: SEUI) -> Bool {
    if se_theme_tokens[7] == 0 || ui.state[13] != 0 { return false; }
    tip :: String := ""; code :: Int64 := se_theme_tokens[7];
    if code == 1 { tip = "Translate / drag an axis"; } if code == 2 { tip = "Rotate / degrees in local axes"; } if code == 3 { tip = "Scale / local axes"; }
    if code == 4 { tip = "Snap: 0.5 units / 15 degrees / 0.1 scale"; } if code == 5 { tip = "Frame selected node / F"; } if code == 6 { tip = "Play or stop the isolated scene / F6"; }
    if code == 7 { tip = "Replace the selected mesh's asset"; } if code == 8 { tip = "Create a mesh under the selected node"; }
    if code == 9 { tip = "Toggle node visibility / inherited by children"; }
    width :: Float64 := se_measure(tip,13)+16.0; mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
    x :: Float64 := vmath.clamp(mouse.x+12.0,4.0,vmath.max(4.0,float64(vraylib.get_screen_width())-width-4.0));
    y :: Float64 := vmath.min(mouse.y+20.0,float64(vraylib.get_screen_height())-30.0);
    box :: vraylib.Rectangle := se_rect(x,y,width,26.0); vraylib.draw_rectangle_rec(box,vraylib.rgba(13,19,29,245));
    vraylib.draw_rectangle_lines_ex(box,1.0,vraylib.rgba(67,87,117,255)); se_draw_text(tip,int64(x+8.0),int64(y+6.0),13,vraylib.LIGHTGRAY); return true;
}
fn se_splitters(ui :: SEUI,width :: Int64,height :: Int64,viewport :: vraylib.Rectangle) -> Bool {
    mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
    if ui.state[13] != 0 { return false; }
    if vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) {
        if vmath.abs(mouse.x-viewport.x) < 4.0 && mouse.y > 42.0 && mouse.y < viewport.y+viewport.height { ui.state[16] = 1; }
        if vmath.abs(mouse.x-viewport.x-viewport.width) < 4.0 && mouse.y > 42.0 && mouse.y < viewport.y+viewport.height { ui.state[17] = 1; }
        if vmath.abs(mouse.y-viewport.y-viewport.height) < 4.0 && mouse.x > viewport.x { ui.state[18] = 1; }
    }
    if vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) {
        if ui.state[16] != 0 { ui.numbers[0] = mouse.x-4.0; }
        if ui.state[17] != 0 { ui.numbers[1] = float64(width)-mouse.x-4.0; }
        if ui.state[18] != 0 { ui.numbers[2] = float64(height)-mouse.y-4.0; }
    } else { ui.state[16] = 0; ui.state[17] = 0; ui.state[18] = 0; }
    return true;
}
