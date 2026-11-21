use "terrain.vy";

# Exact upward normal Y for the same triangle chosen by se_terrain_height.
fn se_terrain_normal_y(world :: SEWorld,x :: Float64,z :: Float64) -> Float64 {
    if !se_terrain_contains(world,x,z) { return 1.0; }
    side :: Int64 := int64(world.data[1]); spacing :: Float64 := world.data[2];
    gx :: Float64 := (x-world.data[3])/spacing; gz :: Float64 := (z-world.data[5])/spacing;
    ix :: Int64 := int64(vmath.min(vmath.floor(gx),float64(side-1))); iz :: Int64 := int64(vmath.min(vmath.floor(gz),float64(side-1))); p :: Int64 := 20496+iz*65+ix;
    ax :: Float64 := (world.data[p+1]-world.data[p])/spacing; az :: Float64 := (world.data[p+65]-world.data[p])/spacing;
    if gx-float64(ix)+gz-float64(iz) > 1.0 { ax = (world.data[p+66]-world.data[p+65])/spacing; az = (world.data[p+66]-world.data[p+1])/spacing; }
    return 1.0/vmath.sqrt(1.0+ax*ax+az*az);
}
fn se_terrain_walkable(world :: SEWorld,x :: Float64,z :: Float64) -> Bool {
    # Explicit 50-degree slope limit, not an elevation-delta proxy.
    return se_terrain_normal_y(world,x,z) >= 0.6427876096865394;
}
fn se_terrain_slope_blocked(world :: SEWorld,from_x :: Float64,from_z :: Float64,to_x :: Float64,to_z :: Float64,feet :: Float64) -> Bool {
    if !se_terrain_contains(world,to_x,to_z) { return false; }
    ground :: Float64 := se_terrain_height(world,to_x,to_z);
    # Airborne bodies above the surface may cross it. Reject ascent/level travel
    # on steep grounded triangles; permit downhill escape from an existing slope.
    if feet > ground+0.3 || se_terrain_walkable(world,to_x,to_z) { return false; }
    if se_terrain_contains(world,from_x,from_z) { if ground < se_terrain_height(world,from_x,from_z)-0.000001 { return false; } }
    return true;
}
# Main can explicitly opt into the generated central pad when starting Play.
# Output into preallocated scratch, never mutate authoring or flatten during Play.
fn se_world_spawn_point(world :: SEWorld,height :: Float64) -> Bool {
    if world.data[9] != 1.0 || world.data[13] <= 0.0 || !se_terrain_contains(world,world.data[14],world.data[15]) { return false; }
    x :: Float64 := world.data[14]; z :: Float64 := world.data[15];
    world.work[4235] = x; world.work[4236] = se_terrain_height(world,x,z)+height; world.work[4237] = z; return true;
}
