use "workspace.vy";
use "../engine/terrain_procedural.vy";

interface SEWorldSettings {
    editor :: SEUI,
    draft :: Array<Float64>,
    state :: Array<Int64>,
    numeric :: Array<Float64>,
}
fn se_world_settings() -> SEWorldSettings {
    text :: Array<Int64> := []; text.reserve(128); text.resize(128,0);
    paste :: Array<Int64> := []; paste.reserve(256); paste.resize(256,0);
    unused :: Array<Int64> := []; unused.reserve(128); unused.resize(128,0);
    flags :: Array<Int64> := []; flags.reserve(64); flags.resize(64,0); flags[1] = -1;
    metrics :: Array<Float64> := []; metrics.reserve(32); metrics.resize(32,0.0);
    editor :: SEUI := SEUI(flags,metrics,text,paste,unused,unused,unused,unused,unused,unused);
    draft :: Array<Float64> := []; draft.reserve(32); draft.resize(32,0.0);
    # dialog 0 closed/1 settings/2 tools/3 presets,validation error,unused,
    # picker result (-1 none),current choice
    numeric :: Array<Float64> := []; numeric.reserve(160); numeric.resize(160,0.0);
    state :: Array<Int64> := [0,0,0,-1,0]; return SEWorldSettings(editor,draft,state,numeric);
}
fn se_world_parameter_slot(row :: Int64) -> Int64 {
    if row == 0 { return 5; } if row == 1 { return 6; } if row == 2 { return 7; } if row == 3 { return 24; }
    if row == 4 { return 21; } if row == 5 { return 22; } if row == 6 { return 23; } if row == 7 { return 25; }
    if row == 8 { return 2; } if row == 9 { return 3; } if row == 10 { return 0; } if row == 11 { return 1; } return 4;
}
fn se_world_parameter_name(row :: Int64) -> String {
    if row == 0 { return "Seed"; } if row == 1 { return "Side"; } if row == 2 { return "Height"; } if row == 3 { return "Spacing"; }
    if row == 4 { return "Frequency"; } if row == 5 { return "Octaves"; } if row == 6 { return "Persistence"; } if row == 7 { return "Spawn pad"; }
    if row == 8 { return "Radius"; } if row == 9 { return "Power"; } if row == 10 { return "Map grid"; } if row == 11 { return "Elevation"; } return "Flatten Y";
}
fn se_world_parameter_hint(row :: Int64) -> String {
    if row == 0 { return "Seed: integer 0..1000000. Same settings produce the same terrain."; }
    if row == 1 { return "Side: integer 1..64 quads per axis; samples are (side+1)^2."; }
    if row == 2 { return "Height: 0..1024 world units. Applied heights remain editable."; }
    if row == 3 { return "Terrain spacing: .125..1024 world units between samples."; }
    if row == 4 { return "Frequency: .001..2 lattice cells/world unit; larger means finer features."; }
    if row == 5 { return "Octaves: integer 1..8. Each octave doubles frequency."; }
    if row == 6 { return "Persistence: 0..1, the relative amplitude of each finer octave."; }
    if row == 7 { return "Spawn pad: 0 disables; otherwise at least 1.5 spacings, max 1024."; }
    if row == 8 { return "Brush radius: .125..1024 world units, with radial falloff."; }
    if row == 9 { return "Brush power: 0..1024 per dab. Flatten/smooth blend clamps to 1."; }
    if row == 10 { return "Map grid: .125..1024 world units. Independent of terrain spacing."; }
    if row == 11 { return "Construction-plane elevation: -100000..100000 world units."; }
    if row == 12 { return "Flatten target: -1024..1024 absolute world Y; eyedropper can set it."; }
    return "Click a value to edit. Ctrl+A selects all; Tab moves between fields.";
}
fn se_world_options_validate(options :: Array) -> Bool {
    data :: Array<Float64> := options;
    if data[5] != vmath.floor(data[5]) || data[6] != vmath.floor(data[6]) || data[22] != vmath.floor(data[22]) { throw "Seed, side and octaves require integers"; }
    se_terrain_validate(int64(data[6]),data[24],data[7],int64(data[5]),int64(data[8]));
    se_generator_validate(data[21],int64(data[22]),data[23],data[25]);
    if data[25] > 0.0 && data[25] < data[24]*1.5 { throw "Spawn pad is smaller than the terrain sample spacing"; }
    if !(data[0] >= 0.125 && data[0] <= 1024.0 && vmath.abs(data[1]) <= 100000.0 && data[2] >= 0.125 && data[2] <= 1024.0 && data[3] >= 0.0 && data[3] <= 1024.0 && vmath.abs(data[4]) <= 1024.0) { throw "Invalid brush/grid/elevation settings"; } return true;
}
fn se_world_settings_begin(settings :: SEWorldSettings,options :: Array) -> Bool {
    source :: Array<Float64> := options; i :: Int64 := 0; while i < 32 { settings.draft[i] = source[i]; i = i+1; }
    settings.state[0] = 1; settings.state[1] = 0; settings.state[2] = 0; settings.state[3] = -1; settings.editor.state[1] = -1; return true;
}
fn se_world_settings_picker(settings :: SEWorldSettings,kind :: Int64,current :: Int64) -> Bool {
    if kind != 2 && kind != 3 { throw "Unknown world picker"; }
    settings.state[0] = kind; settings.state[1] = 0; settings.state[2] = 0; settings.state[3] = -1; settings.state[4] = current; settings.editor.state[1] = -1; return true;
}
fn se_world_settings_cancel(settings :: SEWorldSettings) -> Bool { settings.state[0] = 0; settings.state[2] = 0; settings.state[3] = -1; settings.editor.state[1] = -1; return true; }
fn se_world_settings_set(settings :: SEWorldSettings,row :: Int64,text :: String) -> Bool {
    if row < 0 || row > 12 { throw "Unknown world parameter"; }
    value :: Float64 := vmge_number(text); settings.draft[se_world_parameter_slot(row)] = value; return true;
}
fn se_world_settings_commit_field(settings :: SEWorldSettings) -> Bool {
    row :: Int64 := settings.editor.state[1]; if row == -1 { return true; }
    try { se_world_settings_set(settings,row,se_text_read(settings.editor.text)); settings.state[1] = 0; return true; }
    catch (error) { settings.state[1] = 1; return false; }
}
fn se_world_settings_apply(settings :: SEWorldSettings,options :: Array) -> Bool {
    if !se_world_settings_commit_field(settings) { return false; }
    try { se_world_options_validate(settings.draft); }
    catch (error) { settings.state[1] = 1; return false; }
    destination :: Array<Float64> := options; row :: Int64 := 0;
    while row < 13 { slot :: Int64 := se_world_parameter_slot(row); destination[slot] = settings.draft[slot]; row = row+1; }
    se_world_settings_cancel(settings); settings.state[2] = 1; return true;
}
fn se_world_settings_input(settings :: SEWorldSettings) -> Bool {
    if settings.state[0] == 0 { return false; }
    if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) { se_world_settings_cancel(settings); return true; }
    if settings.state[0] == 1 && settings.editor.state[1] != -1 {
        row :: Int64 := settings.editor.state[1];
        if se_edit_input(settings.editor) && se_world_settings_commit_field(settings) {
            direction :: Int64 := settings.editor.state[30];
            if direction != 0 { row = (row+direction+13)%13; se_edit_begin(settings.editor,row,-1,se_world_decimal_work(settings.numeric,settings.draft[se_world_parameter_slot(row)])); }
            else { settings.editor.state[1] = -1; }
        }
    } return true;
}
fn se_world_digit(digit :: Int64) -> String {
    if digit == 0 { return "0"; } if digit == 1 { return "1"; } if digit == 2 { return "2"; } if digit == 3 { return "3"; } if digit == 4 { return "4"; }
    if digit == 5 { return "5"; } if digit == 6 { return "6"; } if digit == 7 { return "7"; } if digit == 8 { return "8"; } return "9";
}
fn se_world_number(value :: Float64,x :: Int64,y :: Int64,decimals :: Int64) -> Bool {
    magnitude :: Float64 := vmath.abs(value); cursor :: Int64 := x;
    if value < 0.0 { se_draw_text("-",cursor,y,11,vraylib.LIGHTGRAY); cursor = cursor+7; }
    integer :: Int64 := int64(vmath.floor(magnitude)); divisor :: Int64 := 1000000; started :: Bool := false;
    while divisor >= 1 { digit :: Int64 := (integer/divisor)%10; if digit != 0 || started || divisor == 1 { se_draw_text(se_world_digit(digit),cursor,y,11,vraylib.LIGHTGRAY); cursor = cursor+7; started = true; } divisor = divisor/10; }
    if decimals > 0 { se_draw_text(".",cursor,y,11,vraylib.LIGHTGRAY); cursor = cursor+5; fraction :: Float64 := magnitude-vmath.floor(magnitude); i :: Int64 := 0;
        while i < decimals { fraction = fraction*10.0; digit :: Int64 := int64(vmath.floor(fraction+0.00000001)); se_draw_text(se_world_digit(digit%10),cursor,y,11,vraylib.LIGHTGRAY); cursor = cursor+7; fraction = fraction-float64(digit); i = i+1; }
    } return true;
}
fn se_world_choice_label(kind :: Int64,index :: Int64) -> String {
    if kind == 3 { if index == 0 { return "Hills"; } if index == 1 { return "Island"; } if index == 2 { return "Plateau"; } return "Flat"; }
    if index == 0 { return "Select"; } if index == 1 { return "Paint"; } if index == 2 { return "Erase"; } if index == 3 { return "Rectangle"; }
    if index == 4 { return "Eyedropper"; } if index == 5 { return "Raise"; } if index == 6 { return "Lower"; } if index == 7 { return "Flatten"; } return "Smooth";
}
fn se_world_settings_draw(app :: vraylib.App,settings :: SEWorldSettings,options :: Array) -> Bool {
    kind :: Int64 := settings.state[0]; if kind == 0 { return false; }
    width :: Float64 := vmath.min(680.0,float64(app.width())-20.0); x :: Float64 := (float64(app.width())-width)*0.5;
    y :: Float64 := vmath.max(10.0,(float64(app.height())-330.0)*0.5);
    vraylib.draw_rectangle(0,0,app.width(),app.height(),vraylib.rgba(8,12,18,210));
    title :: String := "Terrain and brush settings"; if kind == 2 { title = "Choose world tool"; } if kind == 3 { title = "Choose terrain preset"; }
    se_panel(se_rect(x,y,width,330.0),title);
    if kind == 1 {
        row :: Int64 := 0; column_width :: Float64 := (width-30.0)*0.5;
        while row < 13 {
            slot :: Int64 := se_world_parameter_slot(row); left :: Float64 := x+10.0+float64(row/7)*(column_width+10.0); top :: Float64 := y+36.0+float64(row%7)*29.0;
            rectangle :: vraylib.Rectangle := se_rect(left,top,column_width,25.0); focused :: Bool := settings.editor.state[1] == row;
            value :: String := ""; if focused { value = se_text_read(settings.editor.text); se_edit_metrics(settings.editor); }
            if se_field(se_world_parameter_name(row),value,rectangle,focused,true) {
                if se_world_settings_commit_field(settings) { se_edit_begin(settings.editor,row,-1,se_world_decimal_work(settings.numeric,settings.draft[slot])); }
            }
            if !focused { se_world_number(settings.draft[slot],int64(left+vmath.min(84.0,column_width*0.38)),int64(top+7.0),3); } row = row+1;
        }
        se_draw_text(se_world_parameter_hint(settings.editor.state[1]),int64(x+10.0),int64(y+245.0),12,vraylib.LIGHTGRAY);
        se_draw_text("Enter sets a field; Tab advances. Apply settings, then Preview terrain.",int64(x+10.0),int64(y+263.0),12,vraylib.GRAY);
        if settings.state[1] != 0 { se_draw_text("Invalid value or range. Current tools and world remain unchanged.",int64(x+10.0),int64(y+281.0),12,vraylib.RED); }
        if se_button("Apply settings",se_rect(x+10.0,y+300.0,130.0,24.0),true) { se_world_settings_apply(settings,options); }
    } else {
        count :: Int64 := 9; if kind == 3 { count = 4; } index :: Int64 := 0; cell_width :: Float64 := (width-40.0)/3.0;
        while index < count {
            rectangle :: vraylib.Rectangle := se_rect(x+10.0+float64(index%3)*(cell_width+10.0),y+42.0+float64(index/3)*44.0,cell_width,34.0);
            if se_button(se_world_choice_label(kind,index),rectangle,true) { settings.state[3] = index; settings.state[2] = kind; settings.state[0] = 0; }
            if settings.state[4] == index { vraylib.draw_rectangle_lines_ex(rectangle,2.0,vraylib.SKYBLUE); } index = index+1;
        }
        se_draw_text("Choose a labeled tool directly. Escape closes without changing it.",int64(x+10.0),int64(y+245.0),12,vraylib.GRAY);
    }
    if se_button("Cancel",se_rect(x+width-90.0,y+300.0,80.0,24.0),true) { se_world_settings_cancel(settings); } return true;
}
