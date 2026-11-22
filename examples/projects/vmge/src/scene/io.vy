use "scene.vy";
use "../engine/world_numbers.vy";

fn se_integer(text :: String) -> Int64 {
    value :: Float64 := vmge_number(text);
    if value != vmath.floor(value) { throw "Scene field requires an integer"; }
    return int64(value);
}
fn se_parse(scene :: SEScene, text :: String) -> Bool {
    if text.size() > 1048576 { throw "Scene text exceeds 1 MiB"; }
    column :: Int64 := 0; character :: Int64 := 0;
    while character < text.size() { if text[character] == "\n" { column = 0; } else { column = column+1; if column > 2048 { throw "Scene record exceeds 2048 characters"; } } character = character+1; }
    lines :: Array := vmge_split(text, "\n");
    if lines.size() < 1 || (vmge_trim(lines[0]) != "VMGE_SCENE|1" && vmge_trim(lines[0]) != "VMGE_SCENE|2") { throw "Unsupported scene header/version"; }
    se_clear(scene);
    row :: Int64 := 1;
    while row < lines.size() {
        line :: String := vmge_trim(lines[row]);
        if line != "" && line[0] != "W" && line[0] != "C" && line[0] != "H" {
            try {
                fields :: Array := vmge_split(line, "|");
                if fields.size() != 21 { throw "Expected 21 scene fields"; }
                slot :: Int64 := se_integer(fields[0]);
                parent :: Int64 := se_integer(fields[1]);
                kind :: Int64 := se_integer(fields[2]);
                visible :: Int64 := se_integer(fields[3]);
                texture :: Int64 := se_integer(fields[4]);
                if slot < 0 || slot >= 128 || scene.meta[slot * 8] != 0 { throw "Duplicate or invalid node slot"; }
                if parent < -1 || parent >= 128 || kind < 0 || kind > 4 || visible < 0 || visible > 1 || texture < 0 || texture > 7 { throw "Invalid scene metadata"; }
                scene.meta[slot * 8] = 1; scene.meta[slot * 8 + 1] = 1;
                scene.meta[slot * 8 + 2] = -1; if parent != -1 { scene.meta[slot * 8 + 2] = 128 + parent; }
                scene.meta[slot * 8 + 3] = kind; scene.meta[slot * 8 + 4] = visible; scene.meta[slot * 8 + 5] = texture; scene.meta[slot * 8 + 6] = -1; scene.meta[slot * 8 + 7] = 0;
                if scene.clocks[slot] < 1 { scene.clocks[slot] = 1; } scene.state[0] = scene.state[0] + 1;
                field :: Int64 := 0;
                while field < 14 { scene.property(scene.handle(slot), field, vmge_number(fields[field + 5])); field = field + 1; }
                se_text_unhex(scene.names[scene.offset + slot], fields[19]);
                se_text_unhex(scene.paths[scene.offset + slot], fields[20]);
            } catch (error) { throw "Scene line " + string(row + 1) + ": " + string(error); }
        }
        row = row + 1;
    }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 { scene.reparent(scene.handle(slot), scene.meta[slot * 8 + 2]); }
        slot = slot + 1;
    }
    se_parse_world(scene.world,lines,vmge_trim(lines[0]) == "VMGE_SCENE|2");
    return true;
}
fn se_parse_world(world :: SEWorld,lines :: Array,version2 :: Bool) -> Bool {
    seen :: Bool := false; heights :: Int64 := 0; row :: Int64 := 1;
    i :: Int64 := 0; while i < 4225 { world.work[i] = 0.0; i = i+1; }
    while row < lines.size() {
        line :: String := vmge_trim(lines[row]);
        if line != "" && (line[0] == "W" || line[0] == "C" || line[0] == "H") {
            if !version2 { throw "World records require scene version 2"; }
            fields :: Array := vmge_split(line,"|");
            if fields[0] == "W" {
                if seen || (fields.size() != 8 && fields.size() != 9) { throw "Invalid or duplicate world header"; }
                side :: Int64 := se_integer(fields[1]); spacing :: Float64 := vmge_number(fields[2]);
                seed :: Int64 := se_integer(fields[6]); preset :: Int64 := se_integer(fields[7]);
                if side < 0 { throw "Invalid terrain size"; }
                amplitude :: Float64 := 0.0; if fields.size() == 9 { amplitude = vmge_number(fields[8]); }
                se_terrain_validate(int64(vmath.max(1.0,float64(side))),spacing,amplitude,seed,preset);
                world.data[1] = float64(side); world.data[2] = spacing;
                world.data[3] = vmge_number(fields[3]); world.data[4] = vmge_number(fields[4]); world.data[5] = vmge_number(fields[5]); world.data[6] = float64(seed); world.data[7] = float64(preset); world.data[8] = amplitude; seen = true;
            } else {
                if !seen { throw "World record before header"; }
                if fields[0] == "C" {
                    if fields.size() != 6 || world.count() >= 4096 { throw "Invalid world cell/capacity"; }
                    x :: Float64 := vmge_number(fields[1]); y :: Float64 := vmge_number(fields[2]); z :: Float64 := vmge_number(fields[3]); size :: Float64 := vmge_number(fields[4]); palette :: Int64 := se_integer(fields[5]);
                    vmge_validate_cube(x,y,z,size,palette);
                    p :: Int64 := 16+world.count()*5; world.data[p] = x; world.data[p+1] = y; world.data[p+2] = z; world.data[p+3] = size; world.data[p+4] = float64(palette); world.data[0] = float64(world.count()+1);
                } else {
                    if fields[0] != "H" || fields.size() != 4 || world.side() == 0 { throw "Invalid terrain vertex"; }
                    x :: Int64 := se_integer(fields[1]); z :: Int64 := se_integer(fields[2]); h :: Float64 := vmge_number(fields[3]);
                    if x < 0 || x > world.side() || z < 0 || z > world.side() || !(vmath.abs(h) <= 1024.0) { throw "Terrain vertex out of bounds"; }
                    p :: Int64 := z*65+x; if world.work[p] != 0.0 { throw "Duplicate terrain vertex"; }
                    world.work[p] = 1.0; world.data[20496+p] = h; heights = heights+1;
                }
            }
        } row = row+1;
    }
    if version2 && !seen { throw "Missing world header"; }
    final_side :: Int64 := world.side();
    if final_side > 0 && heights != (final_side+1)*(final_side+1) { throw "Incomplete terrain surface: "+string(heights)+" vertices for side "+string(final_side); }
    return true;
}
fn se_scene_text(scene :: SEScene) -> String {
    builder :: Int64 := vcore.sb_create();
    if scene.world.count() == 0 && scene.world.side() == 0 { vcore.sb_append(builder,"VMGE_SCENE|1\n"); }
    else { vcore.sb_append(builder,"VMGE_SCENE|2\n"); }
    slot :: Int64 := 0;
    while slot < 128 {
        if scene.meta[slot * 8] != 0 {
            parent :: Int64 := scene.meta[slot * 8 + 2]; if parent != -1 { parent = parent % 128; }
            vcore.sb_append(builder, string(slot) + "|" + string(parent) + "|" + string(scene.meta[slot * 8 + 3]) + "|" + string(scene.meta[slot * 8 + 4]) + "|" + string(scene.meta[slot * 8 + 5]));
            field :: Int64 := 0;
            while field < 14 { vcore.sb_append(builder, "|" + se_world_decimal(scene.world,scene.values[slot * 16 + field])); field = field + 1; }
            vcore.sb_append(builder, "|" + se_text_hex(scene.names[scene.offset + slot]) + "|" + se_text_hex(scene.paths[scene.offset + slot]) + "\n");
        }
        slot = slot + 1;
    }
    if scene.world.count() > 0 || scene.world.side() > 0 {
        world :: SEWorld := scene.world;
        vcore.sb_append(builder,"W"); field :: Int64 := 1; while field < 9 { vcore.sb_append(builder,"|"+string(world.data[field])); field = field+1; } vcore.sb_append(builder,"\n");
        i :: Int64 := 0; while i < world.count() { vcore.sb_append(builder,"C"); field = 0; while field < 5 { vcore.sb_append(builder,"|"+string(world.data[16+i*5+field])); field = field+1; } vcore.sb_append(builder,"\n"); i = i+1; }
        if world.side() > 0 { z :: Int64 := 0; while z <= world.side() { x :: Int64 := 0; while x <= world.side() { vcore.sb_append(builder,"H|"+string(x)+"|"+string(z)+"|"+string(world.data[20496+z*65+x])+"\n"); x = x+1; } z = z+1; } }
    }
    return vcore.sb_build(builder);
}
fn se_import_text(destination :: SEScene,staging :: SEScene,text :: String) -> Bool {
    se_parse(staging,text); se_replace(destination,staging); return true;
}
fn se_load(destination :: SEScene, staging :: SEScene, path :: String) -> Bool {
    @bounded<67108864> region import_scene {
        text := vfs.read(path);
        if text == null { throw "Could not read scene: " + path; }
        se_parse(staging, text);
    };
    se_replace(destination, staging);
    return true;
}
fn se_save(scene :: SEScene, path :: String) -> Bool {
    @bounded<67108864> region save_scene {
        if !vfs.write_atomic(path, se_scene_text(scene)) { throw "Could not save scene: " + path; }
    };
    return true;
}
fn se_demo(scene :: SEScene) -> Bool {
    se_clear(scene);
    root :: Int64 := scene.spawn(0, "Demo", -1);
    floor :: Int64 := scene.spawn(1, "Ground", root);
    scene.property(floor, 1, -0.25); scene.property(floor, 6, 24.0); scene.property(floor, 7, 0.5); scene.property(floor, 8, 24.0);
    scene.meta[(floor % 128) * 8 + 5] = 4;
    cube :: Int64 := scene.spawn(1, "Crate", root);
    scene.property(cube, 0, 2.0); scene.property(cube, 1, 1.0); scene.property(cube, 2, 4.0);
    scene.property(cube, 6, 2.0); scene.property(cube, 7, 2.0); scene.property(cube, 8, 2.0);
    scene.meta[(cube % 128) * 8 + 5] = 0;
    actor :: Int64 := scene.spawn(4, "Player", root);
    camera :: Int64 := scene.spawn(2, "Camera", actor); scene.property(camera, 1, 1.8);
    return true;
}
fn se_resource_demo(scene :: SEScene) -> Bool {
    se_demo(scene);
    scene.meta[12] = 0;
    scene.world.data[3] = -16.0; scene.world.data[5] = -16.0;
    se_terrain_generate(scene.world,32,1.0,6.0,42,1);
    se_world_rectangle(scene.world,0,-16,-16,15,15,1.0,-0.5,6,false);
    scene.property(scene.handle(3),0,-10.0); scene.property(scene.handle(3),1,2.0); scene.property(scene.handle(3),2,-10.0);
    return true;
}
