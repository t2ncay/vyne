use "scene.vy";

# Seventeen preacquired ring slots retain sixteen undo steps plus current state.
# Revisions are distinct from ring indices so undo/save/branch dirty state works.
interface SEHistory {
    numbers :: Array,
    texts :: Array,
    state :: Array<Int64>,
    worlds :: Array,
    materials :: Array,
    extensions :: Array,
    dirty() -> Bool {
        bytes :: Array<Int64> := self.texts[self.state[1] % 17];
        return bytes[41985] != self.state[4];
    }
    saved() -> Bool {
        bytes :: Array<Int64> := self.texts[self.state[1] % 17]; self.state[4] = bytes[41985]; return true;
    }
}
fn se_history(numbers :: Array, texts :: Array) -> SEHistory {
    state :: Array<Int64> := [0, 0, 0, 0, -1];
    worlds :: Array := []; worlds.reserve(17);
    i :: Int64 := 0; while i < 17 { data :: Array<Float64> := se_world_buffer(); worlds.push(data); i = i+1; }
    materials :: Array := []; materials.reserve(17); i = 0;
    while i < 17 { materials.push(se_material_snapshot()); i = i+1; }
    extensions :: Array := []; i = 0; while i < 17 { extensions.push(ss_floats(2050)); i = i+1; }
    return SEHistory(numbers, texts, state,worlds,materials,extensions);
}
fn se_numeric_owner_copy(destination :: Array<Float64>,source :: Array<Float64>) -> Bool {
    i :: Int64 := 0;
    while i < 2048 { destination[i] = source[i]; i = i+1; }
    return true;
}
fn se_snapshot(history :: SEHistory, scene :: SEScene, slot :: Int64, revision :: Int64) -> Bool {
    values :: Array<Float64> := history.numbers[slot];
    bytes :: Array<Int64> := history.texts[slot];
    owner :: Array<Float64> := scene.values;
    se_numeric_owner_copy(values,owner);
    i :: Int64 := 0; while i < 1024 { bytes[i] = scene.meta[i]; i = i + 1; }
    i = 0;
    while i < 128 {
        name_bytes :: Array<Int64> := scene.names[scene.offset + i];
        path_bytes :: Array<Int64> := scene.paths[scene.offset + i];
        j :: Int64 := 0;
        while j < 64 { bytes[1024 + i * 320 + j] = name_bytes[j]; j = j + 1; }
        j = 0; while j < 256 { bytes[1024 + i * 320 + 64 + j] = path_bytes[j]; j = j + 1; }
        i = i + 1;
    }
    bytes[41984] = scene.count(); bytes[41985] = revision;
    se_world_copy_data(history.worlds[slot],scene.world.data);
    se_material_capture(history.materials[slot],scene.materials,false);
    se_extension_snapshot(history.extensions[slot],scene,false);
    return true;
}
fn se_history_reset(history :: SEHistory, scene :: SEScene, saved :: Bool) -> Bool {
    history.state[0] = 0; history.state[1] = 0; history.state[2] = 0;
    history.state[3] = history.state[3] + 1;
    se_snapshot(history, scene, 0, history.state[3]);
    history.state[4] = -1; if saved { history.saved(); }
    return true;
}
fn se_record(history :: SEHistory, scene :: SEScene) -> Bool {
    next :: Int64 := history.state[1] + 1;
    history.state[3] = history.state[3] + 1;
    se_snapshot(history, scene, next % 17, history.state[3]);
    history.state[1] = next; history.state[2] = next;
    if next - history.state[0] >= 17 { history.state[0] = next - 16; }
    return true;
}
fn se_restore(history :: SEHistory, scene :: SEScene) -> Bool {
    slot :: Int64 := history.state[1] % 17;
    values :: Array<Float64> := history.numbers[slot];
    bytes :: Array<Int64> := history.texts[slot];
    owner :: Array<Float64> := scene.values;
    se_numeric_owner_copy(owner,values);
    i :: Int64 := 0; while i < 1024 { scene.meta[i] = bytes[i]; i = i + 1; }
    i = 0;
    while i < 128 {
        name_bytes :: Array<Int64> := scene.names[scene.offset + i];
        path_bytes :: Array<Int64> := scene.paths[scene.offset + i];
        j :: Int64 := 0;
        while j < 64 { name_bytes[j] = bytes[1024 + i * 320 + j]; j = j + 1; }
        j = 0; while j < 256 { path_bytes[j] = bytes[1024 + i * 320 + 64 + j]; j = j + 1; }
        i = i + 1;
    }
    scene.state[0] = bytes[41984];
    se_world_copy_data(scene.world.data,history.worlds[slot]);
    se_material_capture(history.materials[slot],scene.materials,true);
    se_extension_snapshot(history.extensions[slot],scene,true);
    return true;
}
fn se_undo(history :: SEHistory, scene :: SEScene) -> Bool {
    if history.state[1] <= history.state[0] { return false; }
    history.state[1] = history.state[1] - 1; return se_restore(history, scene);
}
fn se_extension_snapshot(data :: Array<Float64>,scene :: SEScene,restore :: Bool) -> Bool {
    i :: Int64 := 0; while i < 2048 { if restore { scene.extra[i] = data[i]; } else { data[i] = scene.extra[i]; } i = i+1; }
    i = 0; while i < 2 { if restore { scene.settings[i] = int64(data[2048+i]); } else { data[2048+i] = float64(scene.settings[i]); } i = i+1; } return true;
}
fn se_redo(history :: SEHistory, scene :: SEScene) -> Bool {
    if history.state[1] >= history.state[2] { return false; }
    history.state[1] = history.state[1] + 1; return se_restore(history, scene);
}
