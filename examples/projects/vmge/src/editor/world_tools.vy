use "workspace.vy";
use "world_settings.vy";
use "../engine/world_motion.vy";
use "../engine/world_character_motion.vy";
use "../engine/world_export.vy";
use "../engine/world_resource_io.vy";

interface SEWorldTools {
    state :: Array<Int64>,
    numbers :: Array<Float64>,
    staging :: SEWorld,
    settings :: SEWorldSettings,
}
fn se_world_tools() -> SEWorldTools {
    # mode,stroke,changed,plane,surface,preview,selected cell,start u,start v
    state :: Array<Int64> := [0,0,0,0,0,0,-2,0,0];
    # grid,elevation,radius,strength,flatten height,seed,side,amplitude,preset,
    # current xyz,current uv,last uv,has last,hit validity
    numbers :: Array<Float64> := [1.0,0.5,3.0,0.15,0.0,42.0,32.0,6.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.06,4.0,0.5,1.0,3.0,0.0,0.0,0.0,0.0,0.0,0.0];
    return SEWorldTools(state,numbers,se_world_create(),se_world_settings());
}
fn se_world_mode_name(mode :: Int64) -> String {
    if mode == 0 { return "Select"; } if mode == 1 { return "Paint"; } if mode == 2 { return "Erase"; } if mode == 3 { return "Rectangle"; }
    if mode == 4 { return "Eyedropper"; } if mode == 5 { return "Raise"; } if mode == 6 { return "Lower"; } if mode == 7 { return "Flatten"; } return "Smooth";
}
fn se_world_stroke_begin(tools :: SEWorldTools,world :: SEWorld) -> Bool {
    if tools.state[1] != 0 || tools.state[5] != 0 { return false; }
    se_world_copy(tools.staging,world); tools.state[1] = 1; tools.state[2] = 0; tools.numbers[16] = 0.0;
    tools.state[7] = int64(tools.numbers[12]); tools.state[8] = int64(tools.numbers[13]); return true;
}
fn se_world_stroke_cancel(tools :: SEWorldTools,world :: SEWorld) -> Bool {
    if tools.state[1] != 0 { se_world_copy(world,tools.staging); }
    tools.state[1] = 0; tools.state[2] = 0; tools.numbers[16] = 0.0; return true;
}
fn se_world_stroke_end(tools :: SEWorldTools,scene :: SEScene,history :: SEHistory) -> Bool {
    if tools.state[1] == 0 { return false; }
    changed :: Bool := tools.state[2] != 0; tools.state[1] = 0;
    if changed { se_record(history,scene); } tools.state[2] = 0; return changed;
}
fn se_world_preview(tools :: SEWorldTools,world :: SEWorld) -> Bool {
    if tools.state[1] != 0 { return false; }
    se_world_options_validate(tools.numbers);
    se_world_copy(tools.staging,world);
    se_terrain_generate_procedural(tools.staging,int64(tools.numbers[6]),tools.numbers[24],tools.numbers[7],int64(tools.numbers[5]),int64(tools.numbers[8]),tools.numbers[21],int64(tools.numbers[22]),tools.numbers[23],tools.numbers[25]); tools.state[5] = 1; return true;
}
fn se_world_tools_busy(tools :: SEWorldTools) -> Bool { return tools.state[1] != 0 || tools.state[5] != 0 || tools.settings.state[0] != 0; }
fn se_world_tools_modal_open(tools :: SEWorldTools) -> Bool { return tools.settings.state[0] != 0; }
fn se_world_tools_load_options(tools :: SEWorldTools,world :: SEWorld) -> Bool {
    if world.side() == 0 { return false; }
    tools.numbers[5] = world.data[6]; tools.numbers[6] = world.data[1]; tools.numbers[7] = world.data[8]; tools.numbers[8] = world.data[7]; tools.numbers[24] = world.data[2];
    if world.data[9] == 1.0 { tools.numbers[21] = world.data[10]; tools.numbers[22] = world.data[11]; tools.numbers[23] = world.data[12]; tools.numbers[25] = world.data[13]; }
    return true;
}
fn se_world_tools_modal_input(tools :: SEWorldTools) -> Bool { return se_world_settings_input(tools.settings); }
fn se_world_tools_modal_draw(app :: vraylib.App,tools :: SEWorldTools) -> Bool {
    result :: Bool := se_world_settings_draw(app,tools.settings,tools.numbers);
    if tools.settings.state[2] == 1 { tools.state[5] = 0; tools.settings.state[2] = 0; }
    choice :: Int64 := tools.settings.state[3];
    if choice >= 0 {
        if tools.settings.state[2] == 2 { tools.state[0] = choice; }
        if tools.settings.state[2] == 3 { tools.numbers[8] = float64(choice); tools.state[5] = 0; }
        tools.settings.state[3] = -1; tools.settings.state[2] = 0;
    } return result;
}
fn se_world_preview_apply(tools :: SEWorldTools,scene :: SEScene,history :: SEHistory) -> Bool {
    if tools.state[5] == 0 { return false; }
    se_world_copy(scene.world,tools.staging); tools.state[5] = 0; se_record(history,scene); return true;
}
# World hit normal is used to offset construction onto an adjacent cube face.
# Otherwise intersect the configurable construction plane, including empty space.
fn se_world_tool_hit(tools :: SEWorldTools,world :: SEWorld,ray :: vraylib.Ray) -> Bool {
    tools.numbers[17] = 0.0;
    hit :: Bool := se_world_ray(world,ray); tools.state[6] = -2;
    x :: Float64 := 0.0; y :: Float64 := 0.0; z :: Float64 := 0.0;
    if hit { tools.state[6] = int64(world.work[4232]); }
    if hit && (tools.state[4] != 0 || tools.state[0] == 0 || tools.state[0] == 2 || tools.state[0] == 4 || tools.state[0] >= 5) {
        x = world.work[4226]; y = world.work[4227]; z = world.work[4228];
        if tools.state[0] == 1 || tools.state[0] == 3 { x = x+world.work[4229]*tools.numbers[0]*0.5; y = y+world.work[4230]*tools.numbers[0]*0.5; z = z+world.work[4231]*tools.numbers[0]*0.5; }
        if (tools.state[0] == 2 || tools.state[0] == 4) && tools.state[6] >= 0 { p :: Int64 := 16+tools.state[6]*5; x = world.data[p]; y = world.data[p+1]; z = world.data[p+2]; }
    } else {
        origin :: Float64 := ray.position.y; direction :: Float64 := ray.direction.y;
        if tools.state[3] == 1 { origin = ray.position.z; direction = ray.direction.z; }
        if tools.state[3] == 2 { origin = ray.position.x; direction = ray.direction.x; }
        if vmath.abs(direction) < 0.000001 { return false; }
        t :: Float64 := (tools.numbers[1]-origin)/direction; if t < 0.0 || t > 1000000.0 { return false; }
        x = ray.position.x+ray.direction.x*t; y = ray.position.y+ray.direction.y*t; z = ray.position.z+ray.direction.z*t;
    }
    if tools.state[0] >= 5 && (!hit || world.work[4232] != -1.0) { return false; }
    tools.numbers[9] = x; tools.numbers[10] = y; tools.numbers[11] = z;
    u :: Float64 := x; v :: Float64 := z;
    if tools.state[3] == 1 { v = y; } if tools.state[3] == 2 { u = y; }
    tools.numbers[12] = vmath.floor(u/tools.numbers[0]+0.5); tools.numbers[13] = vmath.floor(v/tools.numbers[0]+0.5); tools.numbers[17] = 1.0; return true;
}
fn se_world_dab(tools :: SEWorldTools,world :: SEWorld) -> Bool {
    mode :: Int64 := tools.state[0]; if tools.numbers[17] == 0.0 { return false; }
    changed :: Bool := false;
    if mode == 1 || mode == 2 {
        x :: Float64 := tools.numbers[12]*tools.numbers[0]; y :: Float64 := tools.numbers[1]; z :: Float64 := tools.numbers[13]*tools.numbers[0];
        if tools.state[3] == 1 { y = z; z = tools.numbers[1]; } if tools.state[3] == 2 { y = x; x = tools.numbers[1]; }
        if tools.state[4] != 0 || mode == 2 { x = vmath.floor(tools.numbers[9]/tools.numbers[0]+0.5)*tools.numbers[0]; y = vmath.floor((tools.numbers[10]-tools.numbers[1])/tools.numbers[0]+0.5)*tools.numbers[0]+tools.numbers[1]; z = vmath.floor(tools.numbers[11]/tools.numbers[0]+0.5)*tools.numbers[0]; }
        if mode == 2 && tools.state[6] >= 0 { p :: Int64 := 16+tools.state[6]*5; x = world.data[p]; y = world.data[p+1]; z = world.data[p+2]; }
        owner :: Array<Float64> := world.data;
        sx :: Float64 := x; sy :: Float64 := y; sz :: Float64 := z;
        if tools.numbers[16] != 0.0 { sx = tools.numbers[18]; sy = tools.numbers[19]; sz = tools.numbers[20]; }
        steps :: Int64 := int64(vmath.ceil(vmath.max(vmath.abs(x-sx),vmath.max(vmath.abs(y-sy),vmath.abs(z-sz)))/tools.numbers[0]));
        if steps > 128 { throw "Stroke segment exceeds 128 grid steps"; } if steps < 1 { steps = 1; }
        step :: Int64 := 1; while step <= steps {
            t :: Float64 := float64(step)/float64(steps); px :: Float64 := sx+(x-sx)*t; py :: Float64 := sy+(y-sy)*t; pz :: Float64 := sz+(z-sz)*t;
            if step < steps { px = vmath.floor(px/tools.numbers[0]+0.5)*tools.numbers[0]; py = vmath.floor((py-tools.numbers[1])/tools.numbers[0]+0.5)*tools.numbers[0]+tools.numbers[1]; pz = vmath.floor(pz/tools.numbers[0]+0.5)*tools.numbers[0]; }
            if mode == 1 { if se_world_paint_data(owner,px,py,pz,tools.numbers[0],int64(world.work[4234])) { changed = true; } }
            else { if se_world_erase_data(owner,px,py,pz) { changed = true; } } step = step+1;
        }
        tools.numbers[18] = x; tools.numbers[19] = y; tools.numbers[20] = z; tools.numbers[16] = 1.0;
    }
    if mode >= 5 { changed = se_terrain_sculpt(world,tools.numbers[9],tools.numbers[11],tools.numbers[2],tools.numbers[3],tools.numbers[4],mode-5); }
    if changed { tools.state[2] = 1; } return changed;
}
fn se_world_tools_input(tools :: SEWorldTools,ui :: SEUI,scene :: SEScene,history :: SEHistory,camera :: vraylib.Camera,viewport :: vraylib.Rectangle) -> Bool {
    if se_world_tools_modal_open(tools) { return true; }
    if ui.state[4] != 0 || ui.state[1] != -1 || ui.state[13] != 0 {
        if tools.state[1] != 0 { se_world_stroke_cancel(tools,scene.world); }
        if ui.state[4] != 0 || ui.state[13] != 0 { tools.state[5] = 0; } return false;
    }
    if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) {
        if tools.state[1] != 0 { se_world_stroke_cancel(tools,scene.world); return true; }
        if tools.state[5] != 0 { tools.state[5] = 0; return true; }
    }
    if tools.state[1] == 0 && se_hover(viewport) {
        if vraylib.is_key_pressed(vraylib.KEY_B) { tools.state[0] = (tools.state[0]+1)%9; }
        if vraylib.is_key_pressed(vraylib.KEY_P) { tools.state[3] = (tools.state[3]+1)%3; }
        if vraylib.is_key_pressed(vraylib.KEY_N) { tools.state[4] = 1-tools.state[4]; }
        if vraylib.is_key_pressed(vraylib.KEY_EQUAL) { tools.numbers[0] = vmath.min(1024.0,tools.numbers[0]*2.0); }
        if vraylib.is_key_pressed(vraylib.KEY_MINUS) { tools.numbers[0] = vmath.max(0.125,tools.numbers[0]*0.5); }
        if vraylib.is_key_pressed(vraylib.KEY_PAGE_UP) { tools.numbers[1] = vmath.min(100000.0,tools.numbers[1]+tools.numbers[0]); }
        if vraylib.is_key_pressed(vraylib.KEY_PAGE_DOWN) { tools.numbers[1] = vmath.max(-100000.0,tools.numbers[1]-tools.numbers[0]); }
    }
    if tools.state[0] == 0 || tools.state[5] != 0 { return false; }
    mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
    if se_hover(viewport) && mouse.y > viewport.y+114.0 {
        ray :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(mouse.x-viewport.x,mouse.y-viewport.y),int64(viewport.width),int64(viewport.height));
        se_world_tool_hit(tools,scene.world,ray);
        if vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) && tools.numbers[17] != 0.0 && !se_view_controls_hit(ui,viewport) {
            if tools.state[0] == 4 {
                if tools.state[6] >= 0 { p :: Int64 := 16+tools.state[6]*5; tools.numbers[0] = vmath.clamp(scene.world.data[p+3],0.125,1024.0); scene.world.work[4234] = scene.world.data[p+4]; tools.numbers[1] = scene.world.data[p+1]; }
                else { tools.numbers[4] = tools.numbers[10]; } return true;
            }
            se_world_stroke_begin(tools,scene.world);
        }
        if tools.state[1] != 0 && vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) { se_world_dab(tools,scene.world); }
    }
    if tools.state[1] != 0 && !vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) {
        try {
            if tools.state[0] == 3 && tools.numbers[17] != 0.0 {
                if se_world_rectangle(scene.world,tools.state[3],tools.state[7],tools.state[8],int64(tools.numbers[12]),int64(tools.numbers[13]),tools.numbers[0],tools.numbers[1],int64(scene.world.work[4234]),vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT)) { tools.state[2] = 1; }
            }
            se_world_stroke_end(tools,scene,history);
        } catch (error) { se_world_stroke_cancel(tools,scene.world); throw error; }
    } return true;
}
fn se_world_tools_draw(tools :: SEWorldTools,world :: SEWorld) -> Bool {
    center := tools.staging.points[0];
    if tools.state[0] == 0 {
        if tools.state[6] >= 0 && tools.state[6] < world.count() {
            p :: Int64 := 16+tools.state[6]*5; center.x = world.data[p]; center.y = world.data[p+1]; center.z = world.data[p+2];
            vraylib.draw_cube_wires(center,world.data[p+3],world.data[p+3],world.data[p+3],vraylib.YELLOW);
        } return true;
    }
    if tools.numbers[17] == 0.0 || tools.state[5] != 0 { return false; }
    center.x = tools.numbers[9]; center.y = tools.numbers[10]; center.z = tools.numbers[11];
    if tools.state[0] >= 5 { vraylib.draw_sphere_wires(center,tools.numbers[2],8,12,vraylib.YELLOW); return true; }
    if tools.state[0] == 3 && tools.state[1] != 0 {
        u :: Float64 := (float64(tools.state[7])+tools.numbers[12])*0.5*tools.numbers[0]; v :: Float64 := (float64(tools.state[8])+tools.numbers[13])*0.5*tools.numbers[0];
        width :: Float64 := (vmath.abs(float64(tools.state[7])-tools.numbers[12])+1.0)*tools.numbers[0]; depth :: Float64 := (vmath.abs(float64(tools.state[8])-tools.numbers[13])+1.0)*tools.numbers[0];
        center.x = u; center.y = tools.numbers[1]; center.z = v;
        if tools.state[3] == 0 { vraylib.draw_cube_wires(center,width,tools.numbers[0],depth,vraylib.YELLOW); }
        if tools.state[3] == 1 { center.y = v; center.z = tools.numbers[1]; vraylib.draw_cube_wires(center,width,depth,tools.numbers[0],vraylib.YELLOW); }
        if tools.state[3] == 2 { center.x = tools.numbers[1]; center.y = u; vraylib.draw_cube_wires(center,tools.numbers[0],width,depth,vraylib.YELLOW); } return true;
    }
    vraylib.draw_cube_wires(center,tools.numbers[0],tools.numbers[0],tools.numbers[0],vraylib.YELLOW); return true;
}
fn se_world_tools_panel(app :: vraylib.App,tools :: SEWorldTools,ui :: SEUI,scene :: SEScene,history :: SEHistory,viewport :: vraylib.Rectangle) -> Bool {
    if ui.state[4] != 0 { return false; }
    clip :: vraylib.Clip := app.clip(viewport); clip.begin();
    try {
    x :: Float64 := viewport.x+6.0; y :: Float64 := viewport.y+5.0;
    scale :: Float64 := vmath.min(1.0,(viewport.width-12.0)/386.0);
    editable :: Bool := ui.state[1] == -1 && ui.state[13] == 0 && tools.state[1] == 0 && !se_world_tools_modal_open(tools);
    if se_button(se_world_mode_name(tools.state[0]),se_rect(x,y,92.0*scale,24.0),editable && tools.state[5] == 0) { se_world_settings_picker(tools.settings,2,tools.state[0]); }
    preset :: String := "Hills"; if tools.numbers[8] == 1.0 { preset = "Island"; } if tools.numbers[8] == 2.0 { preset = "Plateau"; } if tools.numbers[8] == 3.0 { preset = "Flat"; }
    if se_button(preset,se_rect(x+98.0*scale,y,78.0*scale,24.0),editable) { se_world_settings_picker(tools.settings,3,int64(tools.numbers[8])); }
    if se_button("Preview",se_rect(x+182.0*scale,y,70.0*scale,24.0),editable) { se_world_preview(tools,scene.world); }
    if tools.state[5] != 0 {
        if se_button("Apply",se_rect(x+258.0*scale,y,55.0*scale,24.0),editable) { se_world_preview_apply(tools,scene,history); }
        if se_button("Cancel",se_rect(x+319.0*scale,y,60.0*scale,24.0),editable) { tools.state[5] = 0; }
    } else {
        if se_button("Palette",se_rect(x+258.0*scale,y,65.0*scale,24.0),editable) { scene.world.work[4234] = float64((int64(scene.world.work[4234])+1)%8); }
        vraylib.draw_rectangle_rec(se_rect(x+329.0*scale,y+2.0,42.0*scale,20.0),scene.world.colors[int64(scene.world.work[4234])]);
    }
    if se_button("Settings...",se_rect(x,y+28.0,112.0*scale,22.0),editable) { se_world_settings_begin(tools.settings,tools.numbers); }
    se_draw_text("Power",int64(x+119.0*scale),int64(y+34.0),11,vraylib.GRAY); se_world_number(tools.numbers[3],int64(x+157.0*scale),int64(y+34.0),3);
    se_draw_text("Cells",int64(x+218.0*scale),int64(y+34.0),11,vraylib.GRAY); se_world_number(float64(scene.world.count()),int64(x+248.0*scale),int64(y+34.0),0);
    plane :: String := "XZ"; if tools.state[3] == 1 { plane = "XY"; } if tools.state[3] == 2 { plane = "YZ"; }
    if se_button(plane,se_rect(x+278.0*scale,y+28.0,44.0*scale,22.0),editable) { tools.state[3] = (tools.state[3]+1)%3; }
    surface :: String := "Plane"; if tools.state[4] != 0 { surface = "Surface"; }
    if se_button(surface,se_rect(x+326.0*scale,y+28.0,60.0*scale,22.0),editable) { tools.state[4] = 1-tools.state[4]; }
    se_draw_text("Grid",int64(x),int64(y+54.0),11,vraylib.GRAY); se_world_number(tools.numbers[0],int64(x+30.0),int64(y+54.0),3);
    se_draw_text("Elev",int64(x+103.0),int64(y+54.0),11,vraylib.GRAY); se_world_number(tools.numbers[1],int64(x+133.0),int64(y+54.0),3);
    se_draw_text("Seed",int64(x+240.0),int64(y+54.0),11,vraylib.GRAY); se_world_number(tools.numbers[5],int64(x+272.0),int64(y+54.0),0);
    se_draw_text("Side",int64(x),int64(y+69.0),11,vraylib.GRAY); se_world_number(tools.numbers[6],int64(x+30.0),int64(y+69.0),0);
    se_draw_text("Height",int64(x+103.0),int64(y+69.0),11,vraylib.GRAY); se_world_number(tools.numbers[7],int64(x+147.0),int64(y+69.0),1);
    se_draw_text("Radius",int64(x+240.0),int64(y+69.0),11,vraylib.GRAY); se_world_number(tools.numbers[2],int64(x+288.0),int64(y+69.0),1);
    status :: String := "Ready"; if tools.state[1] != 0 { status = "Stroke: release commits / Esc rolls back"; } if tools.state[5] != 0 { status = "Preview only: Apply terrain or Cancel"; }
    se_draw_text(status,int64(x),int64(y+84.0),11,vraylib.SKYBLUE);
    se_draw_text("PgUp/Dn elevation / Shift rectangle erases / Esc cancel",int64(x),int64(y+98.0),11,vraylib.LIGHTGRAY);
    return true;
    } finally { clip.end(); }
}
