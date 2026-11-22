use "assets.vy";
use "map.vy";

# Create reusable textured cube models for all eight palette slots.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     textures (Array): Reusable managed textures indexed by palette slot.
#
# Returns:
#     Array containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn vmge_models(app :: vraylib.App, textures :: Array) -> Array {
    models :: Array := [];
    through texture :: textures -> loop {
        models.push(app.cube(texture));
    };
    return models;
}

# Draw every legacy-map cube with its cached palette model, world-space center, and side length.
#
# Parameters:
#     world (VMGEMap): Fixed-capacity legacy cube map with numeric records and retained imported
#         spellings.
#     models (Array): Reusable managed cube models indexed by texture slot.
#     eye (vraylib.Vector3): Camera eye position in world coordinates.
#     distance (Float64): Draw/cursor distance in world units.
#
# Returns:
#     True when the operation completes without throwing.
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

# Round a world coordinate to the historical two-unit construction grid.
#
# Parameters:
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#     grid (Float64): Construction-grid spacing in world units, from 0.125 through 1,024.
#
# Returns:
#     The computed Float64 value: vmath.round(value / grid) * grid.
#
# Errors:
#     Grid size must be positive.
fn vmge_snap(value :: Float64, grid :: Float64) -> Float64 {
    if grid <= 0.0 {
        throw "Grid size must be positive";
    }
    return vmath.round(value / grid) * grid;
}

# Project the editor camera's cursor ray onto the construction grid and update reusable cursor
# coordinates.
#
# Parameters:
#     camera (vraylib.Camera): Live managed camera used for viewing, movement, or screen projection.
#     width (Int64): Width or component count; rendering dimensions are measured in pixels.
#     height (Int64): Height; player geometry uses world units and render/UI dimensions use pixels.
#     distance (Float64): Draw/cursor distance in world units.
#     grid (Float64): Construction-grid spacing in world units, from 0.125 through 1,024.
#     origin_y (Float64): Vertical phase of the imported two-unit cube grid.
#
# Returns:
#     A temporary vector containing the computed coordinates described above.
fn vmge_grid_cursor(
    camera :: vraylib.Camera,
    width :: Int64,
    height :: Int64,
    distance :: Float64,
    grid :: Float64,
    origin_y :: Float64
) -> vraylib.Vector3 {
    ray :: vraylib.Ray := camera.ray(vraylib.vec2(float64(width) * 0.5, float64(height) * 0.5));
    point :: vraylib.Vector3 := vraylib.math_vector3_add(
        ray.position,
        vraylib.math_vector3_scale(ray.direction, distance)
    );
    return vraylib.vec3(
        vmge_snap(point.x, grid),
        vmge_snap(point.y - origin_y, grid) + origin_y,
        vmge_snap(point.z, grid)
    );
}
