use "../ui/widgets.vy";
use "../scene/assets.vy";

interface SEAxisScore { distance :: Float64, dx :: Float64, dy :: Float64, }

# Slice parameters stay in native-callable numerical kernels, rather than
# mixed boxed editor/scene callbacks. This view never owns or escapes storage.
fn se_valid_viewport(values :: Slice<Float64>) -> Bool {
    if values.size() != 4 { return false; }
    return values[0] >= 0.0 && values[1] >= 0.0 && values[2] >= 1.0 && values[3] >= 1.0;
}

fn se_orbit(ui :: SEUI,camera :: vraylib.Camera,viewport :: vraylib.Rectangle) -> Bool {
    editable :: Bool := ui.state[4] == 0 && ui.state[1] == -1 && ui.state[13] == 0 && ui.state[2] == -1;
    if se_hover(viewport) && editable {
        if vraylib.is_key_pressed(vraylib.KEY_KP_5) { se_view_preset(ui,camera,0); }
        if vraylib.is_key_pressed(vraylib.KEY_KP_7) { se_view_preset(ui,camera,1); }
        if vraylib.is_key_pressed(vraylib.KEY_KP_1) { se_view_preset(ui,camera,2); }
        if vraylib.is_key_pressed(vraylib.KEY_KP_3) { se_view_preset(ui,camera,3); }
    }
    if se_hover(viewport) && editable && !se_view_controls_hit(ui,viewport) {
        delta :: vraylib.Vector2 := vraylib.get_mouse_delta();
        if vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_RIGHT) {
            if delta.x != 0.0 || delta.y != 0.0 { se_view_preset(ui,camera,0); }
            ui.numbers[3] = ui.numbers[3]-delta.x*0.008;
            ui.numbers[4] = vmath.clamp(ui.numbers[4]+delta.y*0.008,-1.4,1.4);
        }
        if vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_MIDDLE) {
            snapshot :: vraylib.Camera3D := camera.view();
            forward :: vraylib.Vector3 := vraylib.direction(snapshot.position,snapshot.target);
            right :: vraylib.Vector3 := vraylib.math_vector3_normalize(vraylib.math_vector3_cross_product(forward,snapshot.up));
            speed :: Float64 := ui.numbers[5]*0.002;
            if ui.view_state[0] != 0 { speed = snapshot.fovy/vmath.max(1.0,viewport.height); }
            up :: vraylib.Vector3 := vraylib.math_vector3_normalize(vraylib.math_vector3_cross_product(right,forward));
            ui.numbers[6] = ui.numbers[6]+(-right.x*delta.x+up.x*delta.y)*speed;
            ui.numbers[7] = ui.numbers[7]+(-right.y*delta.x+up.y*delta.y)*speed;
            ui.numbers[8] = ui.numbers[8]+(-right.z*delta.x+up.z*delta.y)*speed;
        }
        ui.numbers[5] = vmath.clamp(ui.numbers[5]*(1.0-vraylib.get_mouse_wheel_move()*0.1),1.0,2000.0);
    }
    return se_view_apply(ui,camera);
}
fn se_view_preset(ui :: SEUI,camera :: vraylib.Camera,preset :: Int64) -> Bool {
    if preset < 0 || preset > 3 { throw "Viewport preset must be Perspective, Top, Front, or Side"; }
    snapshot :: vraylib.Camera3D := camera.view();
    if snapshot.projection == vraylib.CAMERA_PERSPECTIVE { ui.view_values[0] = snapshot.fovy; }
    ui.view_state[0] = preset; return se_view_apply(ui,camera);
}
fn se_view_apply(ui :: SEUI,camera :: vraylib.Camera) -> Bool {
    target :: vraylib.Vector3 := vraylib.vec3(ui.numbers[6],ui.numbers[7],ui.numbers[8]);
    horizontal :: Float64 := ui.numbers[5]*vmath.cos(ui.numbers[4]);
    eye :: vraylib.Vector3 := vraylib.vec3(target.x+vmath.sin(ui.numbers[3])*horizontal,target.y+vmath.sin(ui.numbers[4])*ui.numbers[5],target.z+vmath.cos(ui.numbers[3])*horizontal);
    up :: vraylib.Vector3 := vraylib.vec3(0.0,1.0,0.0);
    preset :: Int64 := ui.view_state[0];
    if preset == 0 { snapshot :: vraylib.Camera3D := camera.view(); if snapshot.projection == vraylib.CAMERA_PERSPECTIVE { ui.view_values[0] = snapshot.fovy; } }
    if preset == 1 { eye = vraylib.vec3(target.x,target.y+ui.numbers[5],target.z); up = vraylib.vec3(0.0,0.0,-1.0); }
    if preset == 2 { eye = vraylib.vec3(target.x,target.y,target.z+ui.numbers[5]); }
    if preset == 3 { eye = vraylib.vec3(target.x+ui.numbers[5],target.y,target.z); }
    handle :: Int64 := vraylib.require_managed(camera.session,camera.token);
    vraylib.buffer_write(handle,0,vraylib.Camera3D(eye,target,up,ui.view_values[0],vraylib.CAMERA_PERSPECTIVE));
    if preset != 0 { camera.set_orthographic(vmath.max(0.01,ui.numbers[5]*2.0*vmath.tan(ui.view_values[0]*0.00872664626))); }
    return true;
}
fn se_view_controls_bounds(viewport :: vraylib.Rectangle) -> vraylib.Rectangle {
    return se_rect(viewport.x+viewport.width-220.0,viewport.y+viewport.height-30.0,214.0,24.0);
}
fn se_view_controls_hit(ui :: SEUI,viewport :: vraylib.Rectangle) -> Bool {
    return ui.state[4] == 0 && viewport.width >= 224.0 && viewport.height >= 150.0 && se_hover(se_view_controls_bounds(viewport));
}
fn se_view_controls(ui :: SEUI,camera :: vraylib.Camera,viewport :: vraylib.Rectangle) -> Bool {
    if ui.state[4] != 0 || viewport.width < 224.0 || viewport.height < 150.0 { return false; }
    editable :: Bool := ui.state[1] == -1 && ui.state[13] == 0 && ui.state[2] == -1;
    box :: vraylib.Rectangle := se_view_controls_bounds(viewport);
    labels := ["Persp","Top","Front","Side"]; preset :: Int64 := 0;
    while preset < 4 {
        item :: vraylib.Rectangle := se_rect(box.x+float64(preset)*54.0,box.y,52.0,24.0);
        if se_button(labels[preset],item,editable) { se_view_preset(ui,camera,preset); }
        if ui.view_state[0] == preset { vraylib.draw_rectangle_lines_ex(item,2.0,vraylib.SKYBLUE); }
        preset = preset+1;
    }
    return true;
}
fn se_focus(ui :: SEUI,cache :: SETransforms,slot :: Int64) -> Bool {
    position :: vraylib.Vector3 := se_position(cache,slot);
    box :: vraylib.BoundingBox := se_box(cache,slot);
    ui.numbers[6] = position.x; ui.numbers[7] = position.y; ui.numbers[8] = position.z;
    ui.numbers[5] = vmath.max(4.0,vmath.max(box.max.x-box.min.x,vmath.max(box.max.y-box.min.y,box.max.z-box.min.z))*2.0);
    return true;
}
fn se_axis(scene :: SEScene,cache :: SETransforms,id :: Int64,axis :: Int64,mode :: Int64) -> vraylib.Vector3 {
    basis :: vraylib.Matrix := vraylib.math_matrix_identity();
    if mode == 0 { parent :: Int64 := scene.parent(id); if parent != -1 { basis = se_world(scene,cache,parent); } }
    else { basis = se_world(scene,cache,id); }
    unit :: vraylib.Vector3 := vraylib.vec3(0.0,0.0,0.0);
    if axis == 0 { unit.x = 1.0; } if axis == 1 { unit.y = 1.0; } if axis == 2 { unit.z = 1.0; }
    origin :: vraylib.Vector3 := vraylib.math_vector3_transform(vraylib.vec3(0.0,0.0,0.0),basis);
    return vraylib.math_vector3_subtract(vraylib.math_vector3_transform(unit,basis),origin);
}
fn se_screen(camera :: vraylib.Camera,viewport :: vraylib.Rectangle,position :: vraylib.Vector3) -> vraylib.Vector2 {
    local :: vraylib.Vector2 := vraylib.get_world_to_screen_ex(position,camera.view(),int64(viewport.width),int64(viewport.height));
    return vraylib.vec2(local.x+viewport.x,local.y+viewport.y);
}
fn se_gizmo_pick(ui :: SEUI,scene :: SEScene,cache :: SETransforms,camera :: vraylib.Camera,viewport :: vraylib.Rectangle,workspace :: Array<Float64>) -> Int64 {
    selected :: Int64 := ui.state[0];
    if !scene.valid(selected) { return -1; }
    chosen :: Int64 := -1; nearest :: Float64 := 100.0;
    mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
    origin :: vraylib.Vector3 := se_position(cache,selected%128);
    screen :: vraylib.Vector2 := se_screen(camera,viewport,origin);
    workspace[0] = float64(screen.x); workspace[1] = float64(screen.y); workspace[2] = float64(mouse.x); workspace[3] = float64(mouse.y);
    @layout<soa> @zeroed region gizmo_projection {
        scratch scores :: SEAxisScore[3];
        axis :: Int64 := 0;
        while axis < 3 {
            direction :: vraylib.Vector3 := se_axis(scene,cache,selected,axis,ui.state[3]);
            endpoint :: vraylib.Vector3 := vraylib.math_vector3_add(origin,vraylib.math_vector3_scale(vraylib.math_vector3_normalize(direction),ui.numbers[15]));
            end :: vraylib.Vector2 := se_screen(camera,viewport,endpoint);
            dx :: Float64 := end.x-workspace[0]; dy :: Float64 := end.y-workspace[1];
            denominator :: Float64 := dx*dx+dy*dy;
            if denominator > 4.0 {
                fraction :: Float64 := ((workspace[2]-workspace[0])*dx+(workspace[3]-workspace[1])*dy)/denominator;
                fraction = vmath.clamp(fraction,0.2,1.0);
                x :: Float64 := screen.x+dx*fraction-mouse.x; y :: Float64 := screen.y+dy*fraction-mouse.y;
                scores[axis].dx = dx; scores[axis].dy = dy; scores[axis].distance = x*x+y*y;
                if scores[axis].distance < nearest { nearest = scores[axis].distance; chosen = axis; }
            }
            axis = axis + 1;
        }
    };
    return chosen;
}
fn se_gizmo_begin(ui :: SEUI,scene :: SEScene,cache :: SETransforms,camera :: vraylib.Camera,viewport :: vraylib.Rectangle,axis :: Int64) -> Bool {
    selected :: Int64 := ui.state[0];
    origin :: vraylib.Vector3 := se_position(cache,selected%128);
    direction :: vraylib.Vector3 := se_axis(scene,cache,selected,axis,ui.state[3]);
    screen :: vraylib.Vector2 := se_screen(camera,viewport,origin);
    end :: vraylib.Vector2 := se_screen(camera,viewport,vraylib.math_vector3_add(origin,vraylib.math_vector3_scale(vraylib.math_vector3_normalize(direction),ui.numbers[15])));
    mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
    ui.state[2] = axis; ui.state[14] = selected;
    ui.numbers[9] = mouse.x; ui.numbers[10] = mouse.y;
    ui.numbers[11] = scene.number(selected,ui.state[3]*3+axis);
    ui.numbers[12] = end.x-screen.x; ui.numbers[13] = end.y-screen.y;
    ui.numbers[14] = vraylib.math_vector3_length(direction); ui.numbers[17] = 0.0;
    return true;
}
fn se_gizmo_move(ui :: SEUI,scene :: SEScene,mouse :: vraylib.Vector2) -> Bool {
    axis :: Int64 := ui.state[2];
    if axis == -1 { return false; }
    denominator :: Float64 := ui.numbers[12]*ui.numbers[12]+ui.numbers[13]*ui.numbers[13];
    if denominator < 1.0 { return false; }
    fraction :: Float64 := ((mouse.x-ui.numbers[9])*ui.numbers[12]+(mouse.y-ui.numbers[10])*ui.numbers[13])/denominator;
    value :: Float64 := ui.numbers[11];
    if ui.state[3] == 0 { value = value+fraction*ui.numbers[15]/vmath.max(0.001,ui.numbers[14]); if ui.numbers[16] != 0.0 { value = vmath.round(value*2.0)*0.5; } }
    if ui.state[3] == 1 { value = value+fraction*90.0; if ui.numbers[16] != 0.0 { value = vmath.round(value/15.0)*15.0; } }
    if ui.state[3] == 2 { value = vmath.max(0.01,value+fraction*vmath.max(0.1,value)); if ui.numbers[16] != 0.0 { value = vmath.max(0.1,vmath.round(value*10.0)*0.1); } }
    scene.property(ui.state[14],ui.state[3]*3+axis,value);
    ui.numbers[17] = 1.0;
    return true;
}
fn se_gizmo_draw(ui :: SEUI,scene :: SEScene,cache :: SETransforms) -> Bool {
    if !scene.valid(ui.state[0]) { return false; }
    origin :: vraylib.Vector3 := se_position(cache,ui.state[0]%128);
    colors := [vraylib.RED,vraylib.GREEN,vraylib.BLUE];
    axis :: Int64 := 0;
    while axis < 3 {
        direction :: vraylib.Vector3 := vraylib.math_vector3_normalize(se_axis(scene,cache,ui.state[0],axis,ui.state[3]));
        end :: vraylib.Vector3 := vraylib.math_vector3_add(origin,vraylib.math_vector3_scale(direction,ui.numbers[15]));
        tint :: vraylib.Color := colors[axis]; if ui.state[2] == axis { tint = vraylib.YELLOW; }
        vraylib.draw_line_3d(origin,end,tint);
        if ui.state[3] == 2 { vraylib.draw_cube(end,0.18,0.18,0.18,tint); }
        else { vraylib.draw_sphere(end,0.1,tint); }
        if ui.state[3] == 1 {
            normal :: vraylib.Vector3 := vraylib.math_vector3_cross_product(vraylib.vec3(0.0,0.0,1.0),direction);
            if vraylib.math_vector3_length(normal) < 0.001 { normal = vraylib.vec3(0.0,1.0,0.0); }
            vraylib.draw_circle_3d(origin,ui.numbers[15],normal,vmath.acos(vmath.clamp(direction.z,-1.0,1.0))*57.295779513, tint);
        }
        axis = axis + 1;
    }
    return true;
}
