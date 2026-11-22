use "world.vy";
use "world_numbers.vy";

fn se_world_map_text(world :: SEWorld) -> String {
    data :: Array<Float64> := world.data; count :: Int64 := int64(data[0]); builder :: Int64 := vcore.sb_create(); i :: Int64 := 0;
    while i < count { j :: Int64 := 0; while j < 5 { if j > 0 { vcore.sb_append(builder,","); } vcore.sb_append(builder,se_world_decimal(world,data[16+i*5+j])); j = j+1; } vcore.sb_append(builder,";\n"); i = i+1; } return vcore.sb_build(builder);
}
fn se_world_obj_face(builder :: Int64,base :: Int64,a :: Int64,b :: Int64,c :: Int64,d :: Int64) -> Bool {
    vcore.sb_append(builder,"f "+string(base+a)+" "+string(base+b)+" "+string(base+c)+" "+string(base+d)+"\n"); return true;
}
fn se_world_map_obj(world :: SEWorld) -> String {
    data :: Array<Float64> := world.data; count :: Int64 := int64(data[0]); builder :: Int64 := vcore.sb_create(); vcore.sb_append(builder,"# VMGE world map geometry; terrain omitted\n"); i :: Int64 := 0;
    while i < count {
        p :: Int64 := 16+i*5; half :: Float64 := data[p+3]*0.5;
        vcore.sb_append(builder,"g cube_"+string(i)+"_slot_"+string(int64(data[p+4]))+"\n");
        corner :: Int64 := 0; while corner < 8 {
            sx :: Float64 := -1.0; sy :: Float64 := -1.0; sz :: Float64 := -1.0;
            if corner%4 == 1 || corner%4 == 2 { sx = 1.0; } if corner%4 >= 2 { sy = 1.0; } if corner >= 4 { sz = 1.0; }
            vcore.sb_append(builder,"v "+se_world_decimal(world,data[p]+sx*half)+" "+se_world_decimal(world,data[p+1]+sy*half)+" "+se_world_decimal(world,data[p+2]+sz*half)+"\n"); corner = corner+1;
        }
        base :: Int64 := i*8;
        se_world_obj_face(builder,base,1,4,3,2); se_world_obj_face(builder,base,5,6,7,8); se_world_obj_face(builder,base,1,5,8,4);
        se_world_obj_face(builder,base,2,3,7,6); se_world_obj_face(builder,base,1,2,6,5); se_world_obj_face(builder,base,4,8,7,3); i = i+1;
    } return vcore.sb_build(builder);
}
fn se_world_save_map(world :: SEWorld,path :: String) -> Bool {
    @bounded<67108864> region world_dat_export { if !vfs.write(path,se_world_map_text(world)) { throw "Could not save world map"; } }; return true;
}
fn se_world_export_obj(world :: SEWorld,path :: String) -> Bool {
    @bounded<134217728> region world_obj_export { if !vfs.write(path,se_world_map_obj(world)) { throw "Could not export world OBJ"; } }; return true;
}
