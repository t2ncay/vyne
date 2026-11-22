use "text.vy";
use "../engine/map.vy";
use "../engine/terrain.vy";
use "materials.vy";
use "catalog.vy";

# 128 slots. Metadata: alive,generation,parent handle,type,visible,texture,asset,flags.
# Numeric properties: position[3], Euler degrees[3], scale[3], RGBA[4], FOV.
# Node types: 0 Node, 1 Mesh, 2 Camera, 3 Collision, 4 Character, 5 Sky,
# 6 Directional Light, 7 Point Light. Names/defaults are shared with the catalog.
interface SEScene {
    meta :: Array<Int64>,
    values :: Array<Float64>,
    clocks :: Array<Int64>,
    names :: Array,
    paths :: Array,
    offset :: Int64,
    state :: Array<Int64>,
    world :: SEWorld,
    materials :: SEMaterials,
    extra :: Array<Float64>,
    settings :: Array<Int64>,
    count() -> Int64 { return self.state[0]; }
    valid(id :: Int64) -> Bool {
        if id < 128 { return false; }
        slot :: Int64 := id % 128;
        return self.meta[slot * 8] != 0 && self.meta[slot * 8 + 1] == id / 128;
    }
    handle(slot :: Int64) -> Int64 { return self.meta[slot * 8 + 1] * 128 + slot; }
    require(id :: Int64) -> Int64 { if !self.valid(id) { throw "Stale or invalid scene node " + string(id); } return id % 128; }
    parent(id :: Int64) -> Int64 { slot :: Int64 := self.require(id); return self.meta[slot * 8 + 2]; }
    kind(id :: Int64) -> Int64 { slot :: Int64 := self.require(id); return self.meta[slot * 8 + 3]; }
    name(id :: Int64) -> String { slot :: Int64 := self.require(id); return se_text_read(self.names[self.offset + slot]); }
    path(id :: Int64) -> String { slot :: Int64 := self.require(id); return se_text_read(self.paths[self.offset + slot]); }
    number(id :: Int64, field :: Int64) -> Float64 {
        slot :: Int64 := self.require(id);
        if field < 0 || field >= 14 { throw "Scene property out of range"; }
        return self.values[slot * 16 + field];
    }
    property(id :: Int64, field :: Int64, value :: Float64) -> Bool {
        slot :: Int64 := self.require(id);
        if field < 0 || field >= 14 || !(vmath.abs(value) <= 1000000.0) { throw "Invalid numeric scene property"; }
        if field >= 6 && field <= 8 && (value < 0.01 || value > 10000.0) { throw "Scale must be 0.01 through 10000"; }
        if field >= 9 && field <= 12 && (value < 0.0 || value > 255.0) { throw "Color must be 0 through 255"; }
        if field == 13 && (value < 1.0 || value > 179.0) { throw "Camera FOV must be 1 through 179"; }
        self.values[slot * 16 + field] = value;
        return true;
    }
    spawn(kind :: Int64, title :: String, parent :: Int64) -> Int64 {
        if !se_node_kind_valid(kind) { throw "Unknown node type"; }
        if parent != -1 { self.require(parent); }
        slot :: Int64 := 0;
        while slot < 127 && self.meta[slot * 8] != 0 { slot = slot + 1; }
        if self.meta[slot * 8] != 0 { throw "Scene capacity reached (128 nodes)"; }
        generation :: Int64 := self.clocks[slot];
        if self.meta[slot * 8 + 1] > generation { generation = self.meta[slot * 8 + 1]; }
        if generation >= 72057594037927935 { throw "Scene generation exhausted"; }
        se_text_write(self.names[self.offset + slot], title);
        self.clocks[slot] = generation + 1;
        self.meta[slot * 8] = 1;
        self.meta[slot * 8 + 1] = self.clocks[slot];
        self.meta[slot * 8 + 2] = parent;
        self.meta[slot * 8 + 3] = kind;
        self.meta[slot * 8 + 4] = 1;
        self.meta[slot * 8 + 5] = 0;
        self.meta[slot * 8 + 6] = -1;
        self.meta[slot * 8 + 7] = 0;
        field :: Int64 := 0;
        while field < 16 { self.values[slot * 16 + field] = 0.0; field = field + 1; }
        self.values[slot * 16 + 6] = 1.0; self.values[slot * 16 + 7] = 1.0; self.values[slot * 16 + 8] = 1.0;
        self.values[slot * 16 + 9] = 255.0; self.values[slot * 16 + 10] = 255.0; self.values[slot * 16 + 11] = 255.0; self.values[slot * 16 + 12] = 255.0;
        self.values[slot * 16 + 13] = 70.0;
        se_node_extra_reset(self,slot);
        if kind == 5 && self.settings[1] == -1 { self.settings[1] = slot; }
        path_bytes :: Array<Int64> := self.paths[self.offset + slot]; path_bytes[0] = 0;
        material_path :: Array<Int64> := self.materials.paths[slot]; material_path[0] = 0;
        self.state[0] = self.state[0] + 1;
        return self.handle(slot);
    }
    reparent(id :: Int64, parent :: Int64) -> Bool {
        slot :: Int64 := self.require(id);
        current :: Int64 := parent;
        hops :: Int64 := 0;
        while current != -1 {
            if current == id || hops >= 128 { throw "Scene hierarchy cycle"; }
            current = self.parent(current); hops = hops + 1;
        }
        self.meta[slot * 8 + 2] = parent;
        return true;
    }
    descendant(id :: Int64, ancestor :: Int64) -> Bool {
        current :: Int64 := id;
        hops :: Int64 := 0;
        while current != -1 && hops < 128 {
            if current == ancestor { return true; }
            current = self.parent(current); hops = hops + 1;
        }
        return false;
    }
}

fn se_scene(names :: Array, paths :: Array, offset :: Int64) -> SEScene {
    meta :: Array<Int64> := []; meta.reserve(1024); meta.resize(1024, 0);
    values :: Array<Float64> := []; values.reserve(2048); values.resize(2048, 0.0);
    clocks :: Array<Int64> := []; clocks.reserve(128); clocks.resize(128, 0);
    state :: Array<Int64> := [0];
    return SEScene(meta, values, clocks, names, paths, offset, state,se_world_create(),se_materials(),ss_floats(2048),[-1,-1]);
}
# Extension stride 16. Camera: enabled. Light: intensity, radius, ambient.
# Sky: zenith RGB, horizon RGB (0..1), brightness, sun size degrees,
# sun brightness, linked directional slot (-1 auto), ambient.
# Active camera/sky are slot references: generation changes cannot corrupt them.
fn se_node_extra_reset(scene :: SEScene,slot :: Int64) -> Bool {
    b :: Int64 := slot*16; i :: Int64 := 0; while i < 16 { scene.extra[b+i] = 0.0; i = i+1; }
    kind :: Int64 := scene.meta[slot*8+3]; scene.extra[b] = 1.0;
    if kind == 6 || kind == 7 { scene.extra[b+1] = 10.0; scene.extra[b+2] = 0.15; }
    if kind == 5 {
        scene.extra[b] = 0.12; scene.extra[b+1] = 0.32; scene.extra[b+2] = 0.65;
        scene.extra[b+3] = 0.65; scene.extra[b+4] = 0.78; scene.extra[b+5] = 0.95;
        scene.extra[b+6] = 1.0; scene.extra[b+7] = 1.5; scene.extra[b+8] = 2.0;
        scene.extra[b+9] = -1.0; scene.extra[b+10] = 0.15;
    } return true;
}
fn se_extra_set(scene :: SEScene,id :: Int64,field :: Int64,value :: Float64) -> Bool {
    slot :: Int64 := scene.require(id); kind :: Int64 := scene.kind(id);
    if field < 0 || field >= 16 || !(vmath.abs(value) <= 1000000.0) { throw "Invalid extension property"; }
    if kind == 2 { if field != 0 || (value != 0.0 && value != 1.0) { throw "Camera effect enabled must be boolean"; } }
    else { if kind == 5 {
        if field > 10 { throw "Unknown sky property"; }
        if field <= 5 && (value < 0.0 || value > 1.0) { throw "Sky colors must be 0 through 1"; }
        if (field == 6 || field == 8) && (value < 0.0 || value > 32.0) { throw "Sky brightness must be 0 through 32"; }
        if field == 7 && (value < 0.1 || value > 30.0) { throw "Sun size must be 0.1 through 30 degrees"; }
        if field == 9 && (value != vmath.floor(value) || value < -1.0 || value >= 128.0) { throw "Invalid linked sun slot"; }
        if field == 9 && value >= 0.0 { linked :: Int64 := int64(value); if scene.meta[linked*8] == 0 || scene.meta[linked*8+3] != 6 { throw "Sky sun must reference a live directional light"; } }
        if field == 10 && (value < 0.0 || value > 1.0) { throw "Ambient must be 0 through 1"; }
    } else { if kind != 6 && kind != 7 { throw "Node has no environment properties"; }
        if field > 2 || value < 0.0 || (field == 0 && value > 32.0) || (field == 1 && (value < 0.1 || value > 10000.0)) || (field == 2 && value > 1.0) { throw "Invalid light intensity/range/ambient"; }
    } } scene.extra[slot*16+field] = value; return true;
}
fn se_active_node(scene :: SEScene,kind :: Int64) -> Int64 {
    index :: Int64 := 0; if kind == 5 { index = 1; }
    slot :: Int64 := scene.settings[index];
    if slot >= 0 && slot < 128 { if scene.meta[slot*8] != 0 && scene.meta[slot*8+3] == kind { return scene.handle(slot); } }
    slot = 0; while slot < 128 { if scene.meta[slot*8] != 0 && scene.meta[slot*8+3] == kind { return scene.handle(slot); } slot = slot+1; } return -1;
}
fn se_make_active(scene :: SEScene,id :: Int64) -> Bool {
    slot :: Int64 := scene.require(id); kind :: Int64 := scene.kind(id);
    if kind == 2 { scene.settings[0] = slot; } else { if kind != 5 { throw "Only camera or sky can be active"; } scene.settings[1] = slot; } return true;
}
fn se_clear(scene :: SEScene) -> Bool {
    i :: Int64 := 0;
    while i < 128 {
        if scene.meta[i * 8 + 1] > scene.clocks[i] { scene.clocks[i] = scene.meta[i * 8 + 1]; }
        scene.meta[i * 8] = 0; material_path :: Array<Int64> := scene.materials.paths[i]; material_path[0] = 0; i = i + 1;
    }
    scene.state[0] = 0;
    scene.settings[0] = -1; scene.settings[1] = -1;
    scene.materials.resource_state[0] = 0;
    se_world_clear(scene.world);
    return true;
}
fn se_delete(scene :: SEScene, id :: Int64, work :: Array<Int64>) -> Bool {
    scene.require(id);
    i :: Int64 := 0;
    while i < 128 { work[i] = 0; if scene.meta[i * 8] != 0 { if scene.descendant(scene.handle(i), id) { work[i] = 1; } } i = i + 1; }
    i = 0;
    while i < 128 { if work[i] != 0 { scene.meta[i * 8] = 0; scene.state[0] = scene.state[0] - 1; } i = i + 1; }
    i = 0; while i < 2 { if scene.settings[i] >= 0 { if work[scene.settings[i]] != 0 { scene.settings[i] = -1; } } i = i+1; }
    i = 0; while i < 128 { if scene.meta[i*8] != 0 && scene.meta[i*8+3] == 5 { linked :: Int64 := int64(scene.extra[i*16+9]); if linked >= 0 { if work[linked] != 0 { scene.extra[i*16+9] = -1.0; } } } i = i+1; }
    return true;
}
fn se_copy(destination :: SEScene, source :: SEScene) -> Bool {
    i :: Int64 := 0;
    while i < 1024 { destination.meta[i] = source.meta[i]; i = i + 1; }
    # Copying handles must also reserve their generations in the destination.
    # Existing high-water marks survive older snapshots, dead slots and clears.
    i = 0; while i < 128 {
        if destination.meta[i * 8 + 1] > destination.clocks[i] { destination.clocks[i] = destination.meta[i * 8 + 1]; }
        i = i + 1;
    }
    i = 0; while i < 2048 { destination.values[i] = source.values[i]; i = i + 1; }
    i = 0; while i < 2048 { destination.extra[i] = source.extra[i]; i = i+1; }
    destination.settings[0] = source.settings[0]; destination.settings[1] = source.settings[1];
    i = 0;
    while i < 128 {
        se_text_copy(destination.names[destination.offset + i], source.names[source.offset + i]);
        se_text_copy(destination.paths[destination.offset + i], source.paths[source.offset + i]);
        se_material_node_copy(destination.materials,i,source.materials,i);
        i = i + 1;
    }
    destination.state[0] = source.state[0];
    destination.materials.resource_state[0] = source.materials.resource_state[0];
    i = 0; while i < 64 { se_text_copy(destination.materials.resources[i],source.materials.resources[i]); i = i+1; }
    se_world_copy(destination.world,source.world);
    return true;
}
fn se_replace(destination :: SEScene, source :: SEScene) -> Bool {
    # A fresh replacement handle must fit before any destination mutation.
    slot :: Int64 := 0; while slot < 128 {
        if source.meta[slot * 8] != 0 && (destination.clocks[slot] >= 72057594037927935 || source.meta[slot * 8 + 1] >= 72057594037927935) { throw "Scene generation exhausted"; }
        slot = slot + 1;
    }
    se_copy(destination, source);
    i :: Int64 := 0;
    while i < 128 {
        if destination.meta[i * 8] != 0 { destination.clocks[i] = destination.clocks[i] + 1; destination.meta[i * 8 + 1] = destination.clocks[i]; }
        i = i + 1;
    }
    i = 0;
    while i < 128 {
        if destination.meta[i * 8] != 0 && destination.meta[i * 8 + 2] != -1 { destination.meta[i * 8 + 2] = destination.handle(destination.meta[i * 8 + 2] % 128); }
        i = i + 1;
    }
    return true;
}
fn se_scene_validate_utf8(bytes :: Array<Int64>) -> Bool {
    if bytes.size() < 1 { throw "Missing scene text buffer"; }
    length :: Int64 := bytes[0];
    if length < 0 || length >= bytes.size() { throw "Invalid scene text length"; }
    i :: Int64 := 1;
    while i <= length {
        lead :: Int64 := bytes[i]; width :: Int64 := 1;
        if lead >= 194 && lead <= 223 { width = 2; }
        else { if lead >= 224 && lead <= 239 { width = 3; }
        else { if lead >= 240 && lead <= 244 { width = 4; }
        else { if lead < 1 || lead > 127 { throw "Invalid scene UTF-8 lead byte"; } } } }
        if i + width - 1 > length { throw "Truncated scene UTF-8"; }
        j :: Int64 := 1; while j < width { if bytes[i+j] < 128 || bytes[i+j] > 191 { throw "Invalid scene UTF-8 continuation"; } j = j+1; }
        if width == 3 {
            if (lead == 224 && bytes[i+1] < 160) || (lead == 237 && bytes[i+1] >= 160) { throw "Overlong or surrogate scene UTF-8"; }
        }
        if width == 4 {
            if (lead == 240 && bytes[i+1] < 144) || (lead == 244 && bytes[i+1] > 143) { throw "Out-of-range scene UTF-8"; }
        }
        i = i + width;
    } return true;
}
fn se_duplicate(scene :: SEScene, id :: Int64, work :: Array<Int64>) -> Int64 {
    if work.size() < 128 { throw "Duplicate requires 128 work slots"; }
    scene.require(id);
    total :: Int64 := 0;
    i :: Int64 := 0;
    while i < 128 { work[i] = -1; if scene.meta[i * 8] != 0 { if scene.descendant(scene.handle(i), id) { work[i] = -2; total = total + 1; } } i = i + 1; }
    # Assign every destination and generation without reserving/mutating nodes.
    # Original slots are alive, so none can also be one of these destinations.
    next_slot :: Int64 := 0;
    i = 0;
    while i < 128 {
        if work[i] == -2 {
            while next_slot < 128 && scene.meta[next_slot * 8] != 0 { next_slot = next_slot + 1; }
            if next_slot >= 128 { throw "Scene capacity reached"; }
            generation :: Int64 := scene.clocks[next_slot];
            if scene.meta[next_slot * 8 + 1] > generation { generation = scene.meta[next_slot * 8 + 1]; }
            if generation >= 72057594037927935 { throw "Scene generation exhausted"; }
            work[i] = (generation + 1) * 128 + next_slot; next_slot = next_slot + 1;
        }
        i = i + 1;
    }
    # Finish all fallible validation before copying any slot, text or clock.
    i = 0;
    while i < 128 {
        if work[i] >= 128 {
            kind :: Int64 := scene.meta[i * 8 + 3];
            if !se_node_kind_valid(kind) { throw "Invalid duplicate node type"; }
            current :: Int64 := scene.meta[i * 8 + 2]; hops :: Int64 := 0;
            while current != -1 {
                if !scene.valid(current) || current % 128 == i || hops >= 128 { throw "Invalid duplicate hierarchy"; }
                current = scene.meta[(current % 128) * 8 + 2]; hops = hops + 1;
            }
            source_name :: Array<Int64> := scene.names[scene.offset+i]; source_path :: Array<Int64> := scene.paths[scene.offset+i];
            destination_name :: Array<Int64> := scene.names[scene.offset+work[i]%128]; destination_path :: Array<Int64> := scene.paths[scene.offset+work[i]%128];
            se_scene_validate_utf8(source_name); se_scene_validate_utf8(source_path);
            se_material_limit(scene.materials.instances[i]);
            material_path :: Array<Int64> := scene.materials.paths[i]; se_scene_validate_utf8(material_path);
            if source_name[0] >= destination_name.size() || source_path[0] >= destination_path.size() { throw "Duplicate text capacity exceeded"; }
        }
        i = i + 1;
    }
    # Commit uses only prevalidated numeric/byte writes: no spawn, name decoding,
    # UTF-8/native resource operations, text-copy guards or reparent calls remain.
    i = 0;
    while i < 128 {
        if work[i] >= 128 {
            slot :: Int64 := work[i] % 128; parent :: Int64 := scene.meta[i * 8 + 2];
            if parent != -1 { if work[parent % 128] >= 128 { parent = work[parent % 128]; } }
            field :: Int64 := 0; while field < 8 { scene.meta[slot * 8 + field] = scene.meta[i * 8 + field]; field = field + 1; }
            scene.clocks[slot] = work[i] / 128; scene.meta[slot * 8 + 1] = scene.clocks[slot]; scene.meta[slot * 8 + 2] = parent;
            field = 0; while field < 16 { scene.values[slot * 16 + field] = scene.values[i * 16 + field]; field = field + 1; }
            field = 0; while field < 16 { scene.extra[slot*16+field] = scene.extra[i*16+field]; field = field+1; }
            if scene.meta[slot*8+3] == 5 { linked :: Int64 := int64(scene.extra[slot*16+9]); if linked >= 0 { if work[linked] >= 128 { scene.extra[slot*16+9] = float64(work[linked]%128); } } }
            source_name :: Array<Int64> := scene.names[scene.offset+i]; destination_name :: Array<Int64> := scene.names[scene.offset+slot];
            field = 0; while field <= source_name[0] { destination_name[field] = source_name[field]; field = field + 1; }
            source_path :: Array<Int64> := scene.paths[scene.offset+i]; destination_path :: Array<Int64> := scene.paths[scene.offset+slot];
            field = 0; while field <= source_path[0] { destination_path[field] = source_path[field]; field = field + 1; }
            se_material_node_copy(scene.materials,slot,scene.materials,i);
        } i = i + 1;
    }
    scene.state[0] = scene.state[0] + total;
    return work[id % 128];
}
fn se_import_map(destination :: SEScene, world :: VMGEMap) -> Bool {
    se_world_import(destination.world,world);
    return true;
}
