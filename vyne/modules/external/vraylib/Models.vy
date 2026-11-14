use "Types.vy";
use native vraylib;
ruleset { dynamic_casting };
module vraylib;

fn :: vraylib textured_cube(texture :: Int64) -> Int64 {
    mesh :: Int64 := vraylib.gen_mesh_cube(1.0, 1.0, 1.0);
    model :: Int64 := vraylib.load_model_from_mesh(mesh);
    vraylib.model_texture(model, texture, 0);
    return model;
}

fn :: vraylib textured_plane(texture :: Int64, width :: Float64, length :: Float64) -> Int64 {
    mesh :: Int64 := vraylib.gen_mesh_plane(width, length, 1, 1);
    model :: Int64 := vraylib.load_model_from_mesh(mesh);
    vraylib.model_texture(model, texture, 0);
    return model;
}

fn :: vraylib model_at(model :: Int64, position :: vraylib.Vector3, scale :: vraylib.Vector3, tint :: vraylib.Color) -> Bool {
    vraylib.draw_model_ex(model, position, vraylib.Vector3(0.0, 1.0, 0.0), 0.0, scale, tint);
    return true;
}

fn :: vraylib player_box(position :: vraylib.Vector3, width :: Float64, height :: Float64) -> vraylib.BoundingBox {
    return vraylib.BoundingBox(vraylib.Vector3(position.x - width * 0.5, position.y - height, position.z - width * 0.5), vraylib.Vector3(position.x + width * 0.5, position.y, position.z + width * 0.5));
}

fn :: vraylib cube_box(x :: Float64, y :: Float64, z :: Float64, size :: Float64) -> vraylib.BoundingBox {
    half :: Float64 := size * 0.5;
    return vraylib.BoundingBox(vraylib.Vector3(x - half, y - half, z - half), vraylib.Vector3(x + half, y + half, z + half));
}

# LoadModelAnimations returns an owned collection; buffer_read borrows a sequence
# and field(keyframePoses) borrows one pose. The collection owns both views.
fn :: vraylib animation_pose(animation :: Int64, keyframe :: Int64) -> Int64 {
    return vraylib.field(animation, "keyframePoses", keyframe);
}
