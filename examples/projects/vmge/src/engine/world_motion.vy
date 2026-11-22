use "terrain.vy";

# Exact upward normal Y for the same triangle chosen by se_terrain_height.

# Compute the Y component of the active terrain triangle's upward unit normal at an X/Z position.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     x (Float64): World-space X coordinate.
#     z (Float64): World-space Z coordinate.
#
# Returns:
#     Upward unit-normal Y for the sampled triangle; 1 outside the active terrain.
fn se_terrain_normal_y(world :: SEWorld, x :: Float64, z :: Float64) -> Float64 {
    if !se_terrain_contains(world, x, z) {
        return 1.0;
    }
    side :: Int64 := int64(world.data[1]);
    spacing :: Float64 := world.data[2];
    gx :: Float64 := (x - world.data[3]) / spacing;
    gz :: Float64 := (z - world.data[5]) / spacing;
    ix :: Int64 := int64(vmath.min(vmath.floor(gx), float64(side - 1)));
    iz :: Int64 := int64(vmath.min(vmath.floor(gz), float64(side - 1)));
    p :: Int64 := 20496 + iz * 65 + ix;
    ax :: Float64 := (world.data[p + 1] - world.data[p]) / spacing;
    az :: Float64 := (world.data[p + 65] - world.data[p]) / spacing;
    if gx - float64(ix) + gz - float64(iz) > 1.0 {
        ax = (world.data[p + 66] - world.data[p + 65]) / spacing;
        az = (world.data[p + 66] - world.data[p + 1]) / spacing;
    }
    return 1.0 / vmath.sqrt(1.0 + ax * ax + az * az);
}

# Report whether the sampled terrain normal satisfies the controller's 50-degree walkable-slope
# threshold.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     x (Float64): World-space X coordinate.
#     z (Float64): World-space Z coordinate.
#
# Returns:
#     The Boolean result of se_terrain_normal_y(world, x, z) >= 0.6427876096865394.
fn se_terrain_walkable(world :: SEWorld, x :: Float64, z :: Float64) -> Bool {
    # Explicit 50-degree slope limit, not an elevation-delta proxy.
    return se_terrain_normal_y(world, x, z) >= 0.6427876096865394;
}

# Reject steep grounded ascent or level travel while permitting airborne crossing and downhill escape.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     from_x (Float64): Starting world-space X coordinate for the terrain step.
#     from_z (Float64): Starting world-space Z coordinate for the terrain step.
#     to_x (Float64): Candidate world-space X coordinate for the terrain step.
#     to_z (Float64): Candidate world-space Z coordinate for the terrain step.
#     feet (Float64): Player feet height in world units before the terrain motion.
#
# Returns:
#     True for steep grounded ascent/level motion; false for walkable terrain, airborne crossing,
#     downhill escape, or outside terrain.
fn se_terrain_slope_blocked(
    world :: SEWorld,
    from_x :: Float64,
    from_z :: Float64,
    to_x :: Float64,
    to_z :: Float64,
    feet :: Float64
) -> Bool {
    if !se_terrain_contains(world, to_x, to_z) {
        return false;
    }
    ground :: Float64 := se_terrain_height(world, to_x, to_z);
    # Airborne bodies above the surface may cross it. Reject ascent/level travel
    # on steep grounded triangles; permit downhill escape from an existing slope.
    if feet > ground + 0.3 || se_terrain_walkable(world, to_x, to_z) {
        return false;
    }
    if se_terrain_contains(world, from_x, from_z) {
        if ground < se_terrain_height(world, from_x, from_z) - 0.000001 {
            return false;
        }
    }
    return true;
}

# Main can explicitly opt into the generated central pad when starting Play.
# Output into preallocated scratch, never mutate authoring or flatten during Play.

# Resolve a positive generated pad inside active terrain into player eye XYZ in world scratch; this
# helper does not check cube or scene-node clearance.
#
# Parameters:
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#     height (Float64): Player eye-to-feet height in world units.
#
# Returns:
#     True when generator version one has a positive pad inside the terrain, writing eye XYZ to
#     scratch 4235-4237; false otherwise.
fn se_world_spawn_point(world :: SEWorld, height :: Float64) -> Bool {
    if world.data[9] != 1.0 || world.data[13] <= 0.0 || !se_terrain_contains(
        world,
        world.data[14],
        world.data[15]
    ) {
        return false;
    }
    x :: Float64 := world.data[14];
    z :: Float64 := world.data[15];
    world.work[4235] = x;
    world.work[4236] = se_terrain_height(world, x, z) + height;
    world.work[4237] = z;
    return true;
}
