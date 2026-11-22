use "map.vy";

# Geometry-only OBJ, with outward-facing quads and one named group per cube.
fn vmge_map_obj(world :: VMGEMap) -> String {
    builder :: Int64 := vcore.sb_create();
    vcore.sb_append(builder, "# VMGE geometry export; cube centers and sizes in world units\n");
    corners := [[-1.0,-1.0,-1.0], [1.0,-1.0,-1.0], [1.0,1.0,-1.0], [-1.0,1.0,-1.0], [-1.0,-1.0,1.0], [1.0,-1.0,1.0], [1.0,1.0,1.0], [-1.0,1.0,1.0]];
    faces := [[1,4,3,2], [5,6,7,8], [1,5,8,4], [2,3,7,6], [1,2,6,5], [4,8,7,3]];
    i :: Int64 := 0;
    while i < world.count() {
        vcore.sb_append(builder, "g cube_" + string(i) + "_slot_" + string(int64(world.value(i, 4))) + "\n");
        half :: Float64 := world.value(i, 3) * 0.5;
        through corner :: corners -> loop {
            vcore.sb_append(builder, "v " + string(world.value(i, 0) + corner[0] * half) + " " + string(world.value(i, 1) + corner[1] * half) + " " + string(world.value(i, 2) + corner[2] * half) + "\n");
        };
        through face :: faces -> loop {
            vcore.sb_append(builder, "f " + string(i * 8 + face[0]) + " " + string(i * 8 + face[1]) + " " + string(i * 8 + face[2]) + " " + string(i * 8 + face[3]) + "\n");
        };
        i = i + 1;
    }
    return vcore.sb_build(builder);
}

fn vmge_map_export(world :: VMGEMap, path :: String) -> Bool {
    if !vfs.write(path, vmge_map_obj(world)) { throw "Could not export OBJ: " + path; }
    return true;
}
