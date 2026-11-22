use "../scene/io.vy";
use "terrain_procedural.vy";

# Opt-in extended scene entry points; all v1 and existing v2 records are accepted.
# G|1 is an optional v2 extension, not a change to W or node field layouts.
fn se_world_scene_text(scene :: SEScene) -> String {
    # Ordinary scenes retain the v1 field layout; se_scene_text also formats
    # every node number exactly, rather than the runtime's six-digit %g default.
    if scene.world.side() == 0 && scene.world.count() == 0 { return se_scene_text(scene); }
    builder :: Int64 := vcore.sb_create(); if se_scene_has_extensions(scene) { vcore.sb_append(builder,"VMGE_SCENE|4\n"); } else { if se_scene_has_materials(scene) { vcore.sb_append(builder,"VMGE_SCENE|3\n"); } else { vcore.sb_append(builder,"VMGE_SCENE|2\n"); } } world :: SEWorld := scene.world; slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot*8] != 0 {
            parent :: Int64 := scene.meta[slot*8+2]; if parent != -1 { parent = parent%128; }
            vcore.sb_append(builder,string(slot)+"|"+string(parent)+"|"+string(scene.meta[slot*8+3])+"|"+string(scene.meta[slot*8+4])+"|"+string(scene.meta[slot*8+5]));
            field :: Int64 := 0; while field < 14 { vcore.sb_append(builder,"|"+se_world_decimal(world,scene.values[slot*16+field])); field = field+1; }
            vcore.sb_append(builder,"|"+se_text_hex(scene.names[scene.offset+slot])+"|"+se_text_hex(scene.paths[scene.offset+slot])+"\n");
        } slot = slot+1;
    }
    vcore.sb_append(builder,"W"); field :: Int64 := 1; while field < 9 { vcore.sb_append(builder,"|"+se_world_decimal(world,world.data[field])); field = field+1; } vcore.sb_append(builder,"\n");
    vcore.sb_append(builder,se_scene_material_text(scene));
    vcore.sb_append(builder,se_scene_extension_text(scene));
    if world.side() > 0 && world.data[9] != 0.0 { vcore.sb_append(builder,se_generator_record(world)+"\n"); }
    i :: Int64 := 0; count :: Int64 := int64(world.data[0]); while i < count { vcore.sb_append(builder,"C"); field = 0; while field < 5 { vcore.sb_append(builder,"|"+se_world_decimal(world,world.data[16+i*5+field])); field = field+1; } vcore.sb_append(builder,"\n"); i = i+1; }
    side :: Int64 := int64(world.data[1]); if side > 0 { z :: Int64 := 0; while z <= side { x :: Int64 := 0; while x <= side { vcore.sb_append(builder,"H|"+string(x)+"|"+string(z)+"|"+se_world_decimal(world,world.data[20496+z*65+x])+"\n"); x = x+1; } z = z+1; } } return vcore.sb_build(builder);
}
fn se_world_parse_scene(staging :: SEScene,text :: String) -> Bool {
    if text.size() > 16777216 { throw "Scene text exceeds 16 MiB"; }
    column :: Int64 := 0; character :: Int64 := 0;
    while character < text.size() { if text[character] == "\n" { column = 0; } else { column = column+1; if column > 2048 { throw "Scene record exceeds 2048 characters"; } } character = character+1; }
    rows :: Array := vmge_split(text,"\n"); builder :: Int64 := vcore.sb_create(); record :: String := ""; i :: Int64 := 0;
    while i < rows.size() {
        row :: String := vmge_trim(rows[i]);
        if row != "" && row[0] == "G" { if record != "" { throw "Duplicate generator record"; } record = row; }
        else { vcore.sb_append(builder,row); vcore.sb_append(builder,"\n"); } i = i+1;
    }
    se_parse(staging,vcore.sb_build(builder));
    if record != "" {
        if (vmge_trim(rows[0]) != "VMGE_SCENE|2" && vmge_trim(rows[0]) != "VMGE_SCENE|3" && vmge_trim(rows[0]) != "VMGE_SCENE|4") || staging.world.side() == 0 { throw "Generator record requires a v2/v3/v4 terrain resource"; }
        se_generator_parse_record(staging.world,record);
    } return true;
}
fn se_world_import_scene(destination :: SEScene,staging :: SEScene,text :: String) -> Bool {
    se_world_parse_scene(staging,text); se_replace(destination,staging); return true;
}
fn se_world_load_scene(destination :: SEScene,staging :: SEScene,path :: String) -> Bool {
    @bounded<134217728> region procedural_scene_import {
        text := vfs.read(path); if text == null { throw "Could not read world scene"; } se_world_parse_scene(staging,text);
    }; se_replace(destination,staging); return true;
}
fn se_world_save_scene(scene :: SEScene,path :: String) -> Bool {
    @bounded<67108864> region procedural_scene_save { if !vfs.write_atomic(path,se_world_scene_text(scene)) { throw "Could not save world scene"; } }; return true;
}
