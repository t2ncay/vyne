use "Types.vy";
use native vraylib;
ruleset { dynamic_casting };
module vraylib;

# Mutable cameras live in typed native buffers. A frame's temporary Camera3D
# records may be rewound without invalidating the camera itself.
fn :: vraylib camera_create(position :: vraylib.Vector3, target :: vraylib.Vector3, fovy :: Float64) -> Int64 {
    handle :: Int64 := vraylib.buffer("Camera3D", 1);
    camera := vraylib.Camera3D(position, target, vraylib.Vector3(0.0, 1.0, 0.0), fovy, vraylib.CAMERA_PERSPECTIVE);
    vraylib.buffer_write(handle, 0, camera);
    return handle;
}

fn :: vraylib camera_view(handle :: Int64) -> vraylib.Camera3D {
    return vraylib.buffer_read(handle, 0);
}

fn :: vraylib camera_position(handle :: Int64) -> vraylib.Vector3 {
    camera :: vraylib.Camera3D := vraylib.camera_view(handle);
    return camera.position;
}

fn :: vraylib camera_set_position(handle :: Int64, position :: vraylib.Vector3) -> Bool {
    camera :: vraylib.Camera3D := vraylib.camera_view(handle);
    delta :: vraylib.Vector3 := vraylib.math_vector3_subtract(position, camera.position);
    camera.position = position;
    camera.target = vraylib.math_vector3_add(camera.target, delta);
    vraylib.buffer_write(handle, 0, camera);
    return true;
}

fn :: vraylib camera_set_height(handle :: Int64, height :: Float64) -> Bool {
    position :: vraylib.Vector3 := vraylib.camera_position(handle);
    return vraylib.camera_set_position(handle, vraylib.Vector3(position.x, height, position.z));
}

fn :: vraylib camera_step(handle :: Int64, forward :: Float64, right :: Float64) -> Bool {
    camera :: vraylib.Camera3D := vraylib.camera_view(handle);
    facing :: vraylib.Vector3 := vraylib.math_vector3_subtract(camera.target, camera.position);
    facing.y = 0.0;
    facing = vraylib.math_vector3_normalize(facing);
    side :: vraylib.Vector3 := vraylib.math_vector3_cross_product(facing, camera.up);
    movement :: vraylib.Vector3 := vraylib.math_vector3_add(vraylib.math_vector3_scale(facing, forward), vraylib.math_vector3_scale(side, right));
    return vraylib.camera_set_position(handle, vraylib.math_vector3_add(camera.position, movement));
}

fn :: vraylib camera_mouse_look(handle :: Int64, sensitivity :: Float64) -> Bool {
    delta :: vraylib.Vector2 := vraylib.get_mouse_delta();
    vraylib.update_camera_pro(handle, vraylib.Vector3(0.0, 0.0, 0.0), vraylib.Vector3(delta.x * sensitivity, delta.y * sensitivity, 0.0), 0.0);
    return true;
}
