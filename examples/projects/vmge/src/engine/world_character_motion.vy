use "world_motion.vy";
use "../scene/assets.vy";
use "collision.vy";

# Integrated movement query for main's per-axis Play probes. Return true to
# reject that axis, otherwise use world.work[4238] as the accepted camera Y.
# This replaces the terrain .15-delta proxy for grounded movement, while retaining
# map/node solid collisions and exact heightfield support. Authoring is untouched.

# Test a character motion candidate against map cells, scene bounds, and walkable terrain; publish the
# accepted eye height into world scratch.
#
# Parameters:
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     cache (SETransforms): Per-slot world-matrix/bounds cache; update it before drawing, picking, or
#         collision queries.
#     previous (vraylib.Vector3): Player eye position before applying the candidate motion.
#     candidate (vraylib.Vector3): Proposed player eye position in world coordinates.
#     height (Float64): Height; player geometry uses world units and render/UI dimensions use pixels.
#     exclude (Int64): Character subtree handle to omit from collision queries; -1 excludes nothing.
#     grounded (Bool): Whether the player currently has support for terrain-following motion.
#
# Returns:
#     true when !follows && eye - height < next_height - 0.15. Otherwise returns the completed
#     action/predicate result.
fn se_world_motion_blocked(
    scene :: SEScene,
    cache :: SETransforms,
    previous :: vraylib.Vector3,
    candidate :: vraylib.Vector3,
    height :: Float64,
    exclude :: Int64,
    grounded :: Bool
) -> Bool {
    eye :: Float64 := candidate.y;
    feet :: Float64 := previous.y - height;
    world :: SEWorld := scene.world;
    if se_terrain_contains(world, candidate.x, candidate.z) {
        next_height :: Float64 := se_terrain_height(world, candidate.x, candidate.z);
        if se_terrain_slope_blocked(world, previous.x, previous.z, candidate.x, candidate.z, feet) {
            return true;
        }
        follows :: Bool := false;
        if grounded && se_terrain_contains(world, previous.x, previous.z) {
            before_height :: Float64 := se_terrain_height(world, previous.x, previous.z);
            if vmath.abs(feet - before_height) <= 0.3 {
                dx :: Float64 := candidate.x - previous.x;
                dz :: Float64 := candidate.z - previous.z;
                if next_height - before_height > vmath.sqrt(dx * dx + dz * dz) * 1.19175359259421 + 0.000001 {
                    return true;
                }
                eye = next_height + height;
                follows = true;
            }
        }
        if !follows && eye - height < next_height - 0.15 {
            return true;
        }
    }
    body :: vraylib.BoundingBox := vmge_player_box(vraylib.vec3(candidate.x, eye, candidate.z), height);
    data :: Array<Float64> := world.data;
    count :: Int64 := int64(data[0]);
    i :: Int64 := 0;
    while i < count {
        p :: Int64 := 16 + i * 5;
        r :: Float64 := data[p + 3] * 0.5;
        if body.min.x <= data[p]
            +r
            && body.max.x >= data[p] - r
            && body.min.y <= data[p + 1]
            +r
            && body.max.y >= data[p + 1] - r
            && body.min.z <= data[p + 2]
            +r
            && body.max.z >= data[p + 2] - r {
            return true;
        }
        i = i + 1;
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 && (scene.meta[slot * 8 + 3] == 1 || scene.meta[slot * 8 + 3] == 3) {
            collidable :: Bool := true;
            if exclude != -1 {
                if scene.descendant(scene.handle(slot), exclude) {
                    collidable = false;
                }
            }
            if collidable {
                if vraylib.check_collision_boxes(body, se_box(cache, slot)) {
                    return true;
                }
            }
        }
        slot = slot + 1;
    }
    world.work[4238] = eye;
    return false;
}
