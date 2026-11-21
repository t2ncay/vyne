use "assets.vy";
use "map.vy";

fn vmge_models(app :: vraylib.App, textures :: Array) -> Array {
    models :: Array := [];
    through texture :: textures -> loop { models.push(app.cube(texture)); };
    return models;
}

fn vmge_draw_map(world :: VMGEMap, models :: Array, eye :: vraylib.Vector3, distance :: Float64) -> Bool {
    i :: Int64 := 0;
    while i < world.count() {
        x :: Float64 := world.value(i, 0);
        y :: Float64 := world.value(i, 1);
        z :: Float64 := world.value(i, 2);
        size :: Float64 := world.value(i, 3);
        dx :: Float64 := eye.x - x;
        dz :: Float64 := eye.z - z;
        if distance <= 0.0 || dx * dx + dz * dz <= (distance + size) * (distance + size) {
            model :: vraylib.Model := models[int64(world.value(i, 4))];
            model.draw(vraylib.vec3(x, y, z), size, vraylib.WHITE);
        }
        i = i + 1;
    }
    return true;
}

fn vmge_snap(value :: Float64, grid :: Float64) -> Float64 {
    if grid <= 0.0 { throw "Grid size must be positive"; }
    return vmath.round(value / grid) * grid;
}
fn vmge_grid_cursor(camera :: vraylib.Camera, width :: Int64, height :: Int64, distance :: Float64, grid :: Float64, origin_y :: Float64) -> vraylib.Vector3 {
    ray :: vraylib.Ray := camera.ray(vraylib.vec2(float64(width) * 0.5, float64(height) * 0.5));
    point :: vraylib.Vector3 := vraylib.math_vector3_add(ray.position, vraylib.math_vector3_scale(ray.direction, distance));
    return vraylib.vec3(vmge_snap(point.x, grid), vmge_snap(point.y - origin_y, grid) + origin_y, vmge_snap(point.z, grid));
}
