use "../src/engine/world_export.vy";
use "../src/engine/terrain.vy";
fn world_exports_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
fn world_obj_check(world :: SEWorld,obj :: String) -> Bool {
    vertices :: Array<Float64> := []; vertices.reserve(48); vertices.resize(48,0.0);
    lines :: Array := vmge_split(obj,"\n"); vertex_count :: Int64 := 0; faces :: Int64 := 0; groups :: Int64 := 0; row :: Int64 := 0;
    while row < lines.size() {
        fields :: Array := vmge_split(lines[row]," ");
        if fields[0] == "g" { groups = groups+1; }
        if fields[0] == "v" { j :: Int64 := 0; while j < 3 { vertices[vertex_count*3+j] = vmge_number(fields[j+1]); j = j+1; } vertex_count = vertex_count+1; }
        if fields[0] == "f" {
            a :: Int64 := int64(vmge_number(fields[1]))-1; b :: Int64 := int64(vmge_number(fields[2]))-1; c :: Int64 := int64(vmge_number(fields[3]))-1; d :: Int64 := int64(vmge_number(fields[4]))-1;
            world_exports_check(a >= 0 && b >= 0 && c >= 0 && d >= 0 && a < 16 && b < 16 && c < 16 && d < 16 && a/8 == b/8 && a/8 == c/8 && a/8 == d/8,"OBJ faces reference one cube only");
            ux :: Float64 := vertices[b*3]-vertices[a*3]; uy :: Float64 := vertices[b*3+1]-vertices[a*3+1]; uz :: Float64 := vertices[b*3+2]-vertices[a*3+2];
            vx :: Float64 := vertices[c*3]-vertices[a*3]; vy :: Float64 := vertices[c*3+1]-vertices[a*3+1]; vz :: Float64 := vertices[c*3+2]-vertices[a*3+2]; p :: Int64 := 16+(a/8)*5;
            dot :: Float64 := (uy*vz-uz*vy)*(vertices[a*3]-world.data[p])+(uz*vx-ux*vz)*(vertices[a*3+1]-world.data[p+1])+(ux*vy-uy*vx)*(vertices[a*3+2]-world.data[p+2]);
            world_exports_check(dot > 0.0,"OBJ outward winding"); faces = faces+1;
        } row = row+1;
    }
    world_exports_check(vertex_count == 16 && faces == 12 && groups == 2 && vertices[0] == -3.0 && vertices[1] == 0.5 && vertices[2] == 3.0,"OBJ cube counts and world bounds"); return true;
}
@bounded<134217728> region world_export_owner {
    world :: SEWorld := se_world_create(); se_world_paint(world,-2.0,1.5,4.0,2.0,3); se_world_paint(world,0.25,-3.0,-5.5,1.5,7);
    se_terrain_generate(world,8,1.0,5.0,42,0);
    text :: String := se_world_map_text(world); imported :: VMGEMap := vmge_map_parse(text,2);
    world_exports_check(imported.count() == 2 && imported.value(1,0) == 0.25 && imported.value(1,3) == 1.5 && imported.value(1,4) == 7.0,"legacy DAT numeric roundtrip");
    world_obj_check(world,se_world_map_obj(world));
    precise :: Array<Float64> := [0.12345678901234567,0.99999999999999989,999999.9999999999,1e-300,4.9406564584124654e-324,-1e-100];
    index :: Int64 := 0; while index < precise.size() { world_exports_check(vmge_number(se_world_decimal(world,precise[index])) == precise[index],"17-digit exact Float64 serializer including subnormals"); index = index+1; }
    prefix :: String := "build/world-dev-exports";
    option := vraylib.environment("VYNE_VMGE_WORLD_EXPORT_PREFIX"); if option != null && option != "" { prefix = option; }
    se_world_save_map(world,prefix+".dat"); se_world_export_obj(world,prefix+".obj");
    loaded := vfs.read(prefix+".dat"); world_exports_check(loaded != null && loaded == text,"DAT disk output");
    edge :: SEWorld := se_world_create(); se_world_paint(edge,1000000.0,0.0,0.0,1000000.0,0);
    edge_obj :: String := se_world_map_obj(edge); world_exports_check(edge_obj.size() > 0 && se_world_coordinate("1500000.0") == 1500000.0,"valid large cube corners are exportable beyond center bound");
    se_world_clear(world); se_world_rectangle(world,0,0,0,63,63,1.0,0.5,3,false);
    se_world_save_map(world,prefix+"-maximum.dat"); se_world_export_obj(world,prefix+"-maximum.obj");
    world_exports_check(world.count() == 4096 && world.side() == 0,"maximum exports do not mutate resource");
};
out("VMGE_WORLD_EXPORTS_OK");
