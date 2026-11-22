use "../shader/material.vy";

# Host limits: 128 nodes, 16 overrides (64 components each), 16 samplers,
# 255-byte stable shader paths. All banks are constructed before frames.
interface SEMaterials { instances :: Array, paths :: Array, resources :: Array, resource_state :: Array<Int64>, }
interface SEMaterialSnapshot { integers :: Array<Int64>, numbers :: Array<Float64>, paths :: Array<Int64>, resources :: Array<Int64>, state :: Array<Int64>, }
interface SEPrograms { documents :: Array, paths :: Array, state :: Array<Int64>, }
fn se_programs() -> SEPrograms {
    # One length prefix + 65,535 source bytes fits a 65,536-element buffer.
    # Avoid doubling retained program sources to 131,072 elements; the
    # editable studio document keeps its existing full 64 KiB source limit.
    docs :: Array := []; docs.reserve(10); i :: Int64 := 0;
    while i < 8 { docs.push(ss_document_with_limits(65535,2048,64)); i = i+1; } docs.push(null); docs.push(null);
    paths :: Array := [ss_text(2048),ss_text(2048),ss_text(2048),ss_text(2048),ss_text(2048),ss_text(2048),ss_text(2048),ss_text(2048)];
    return SEPrograms(docs,paths,ss_ints(1));
}
fn se_program_resolve(bank :: SEPrograms,app :: vraylib.App,path :: String) -> Int64 {
    full :: String := ss_normal_path(path);
    i :: Int64 := 0; while i < bank.state[0] { if ss_normal_path(se_text_read(bank.paths[i])) == full { return i; } i = i+1; }
    if i == 8 { throw "Scene shader registry capacity: 8 published resources (restart to reclaim history)"; }
    d :: SSDocument := bank.documents[i]; ss_document_load(d,full);
    if se_material_publication(path) {
        if vfs.basename(full) != "host-"+string(d.state[7])+".vshader" { throw "Shader publication content does not match its immutable resource hash"; }
    }
    if !ss_compile(app,d) { throw "Scene material compile failed: "+se_text_read(d.log); }
    se_text_write(bank.paths[i],full); bank.state[0] = i+1; return i;
}
fn se_programs_close(bank :: SEPrograms) -> Bool {
    i :: Int64 := 0; while i < 8 { d :: SSDocument := bank.documents[i]; ss_document_close(d); if !ss_document_collect(d) { throw "Scene shader dependencies remain at shutdown"; } i = i+1; } return true;
}
fn se_materials() -> SEMaterials {
    instances :: Array := []; paths :: Array := []; instances.reserve(128); paths.reserve(128);
    i :: Int64 := 0; while i < 128 { instances.push(ss_material_sized(1,i+1,16,16)); paths.push(ss_text(255)); i = i+1; }
    resources :: Array := []; resources.reserve(64); i = 0; while i < 64 { resources.push(ss_text(255)); i = i+1; }
    return SEMaterials(instances,paths,resources,ss_ints(1));
}
fn se_material_limit(m :: MaterialInstance) -> Bool {
    if m.state[2] > 16 || m.binding_state[0] > 16 { throw "Scene material limit: 16 parameters / 16 samplers"; } return true;
}
fn se_material_copy(to :: MaterialInstance,from :: MaterialInstance) -> Bool {
    return ss_material_copy(to,from);
}
fn se_material_node_copy(to :: SEMaterials,slot :: Int64,from :: SEMaterials,source :: Int64) -> Bool {
    se_material_limit(from.instances[source]);
    se_material_copy(to.instances[slot],from.instances[source]);
    m :: MaterialInstance := to.instances[slot]; m.state[1] = slot+1;
    se_text_copy(to.paths[slot],from.paths[source]); return true;
}
fn se_material_publication(path :: String) -> Bool {
    name :: String := vfs.basename(path);
    if name.size() < 14 || vfs.extension(name) != ".vshader" { return false; }
    return vraylib.text_subtext(name,0,5) == "host-";
}
fn se_material_guard_source(d :: SSDocument,path :: String) -> Bool {
    if !se_material_publication(path) { return true; }
    raw := vfs.read(path); if raw == null { throw "Cannot save to a content-addressed publication path"; }
    if raw != ss_document_text(d) { throw "Immutable shader publication: open an editable studio copy before saving"; }
    return true;
}
fn se_material_reset(m :: MaterialInstance,document_id :: Int64,instance_id :: Int64) -> Bool {
    i :: Int64 := 0; while i < m.state.size() { m.state[i] = 0; i = i+1; }
    m.state[0] = document_id; m.state[1] = instance_id;
    i = 0; while i < m.meta.size() { m.meta[i] = 0; i = i+1; }
    i = 0; while i < m.values.size() { m.values[i] = 0.0; i = i+1; }
    i = 0; while i < m.bindings.size() { m.bindings[i] = 0; i = i+1; }
    m.binding_state[0] = 0; return true;
}
# Separate power-of-two path/resource sections avoid rounding 68,225 Int64s
# up to 131,072 per snapshot. Numeric values remain an exact power-of-two bank.
fn se_material_snapshot() -> SEMaterialSnapshot { return SEMaterialSnapshot(ss_ints(128*149),ss_floats(128*1024),ss_ints(128*256),ss_ints(64*256),ss_ints(1)); }
fn se_material_capture(snapshot :: SEMaterialSnapshot,bank :: SEMaterials,restore :: Bool) -> Bool {
    slot :: Int64 := 0; while slot < 128 {
        m :: MaterialInstance := bank.instances[slot]; path :: Array<Int64> := bank.paths[slot]; b :: Int64 := slot*149;
        if !restore { se_material_limit(m); }
        i :: Int64 := 0; while i < 4 { if restore { m.state[i] = snapshot.integers[b+i]; } else { snapshot.integers[b+i] = m.state[i]; } i = i+1; }
        i = 0; while i < 80 { if restore { m.meta[i] = snapshot.integers[b+4+i]; } else { snapshot.integers[b+4+i] = m.meta[i]; } i = i+1; }
        i = 0; while i < 64 { if restore { m.bindings[i] = snapshot.integers[b+84+i]; } else { snapshot.integers[b+84+i] = m.bindings[i]; } i = i+1; }
        if restore { m.binding_state[0] = snapshot.integers[b+148]; } else { snapshot.integers[b+148] = m.binding_state[0]; }
        i = 0; while i < 256 { if restore { path[i] = snapshot.paths[slot*256+i]; } else { snapshot.paths[slot*256+i] = path[i]; } i = i+1; }
        i = 0; while i < 1024 { if restore { m.values[i] = snapshot.numbers[slot*1024+i]; } else { snapshot.numbers[slot*1024+i] = m.values[i]; } i = i+1; }
        slot = slot+1;
    }
    if restore { bank.resource_state[0] = snapshot.state[0]; } else { snapshot.state[0] = bank.resource_state[0]; }
    slot = 0; while slot < 64 { path :: Array<Int64> := bank.resources[slot]; i :: Int64 := 0; while i < 256 {
        if restore { path[i] = snapshot.resources[slot*256+i]; } else { snapshot.resources[slot*256+i] = path[i]; } i = i+1;
    } slot = slot+1; } return true;
}
fn se_material_resource(bank :: SEMaterials,path :: String) -> Int64 {
    if path.size() > 255 { throw "Texture resource path exceeds 255 bytes"; }
    i :: Int64 := 0; while i < bank.resource_state[0] { if se_text_read(bank.resources[i]) == path { return i+100; } i = i+1; }
    if i == 64 { throw "Scene texture resource capacity: 64"; }
    se_text_write(bank.resources[i],path); bank.resource_state[0] = i+1; return i+100;
}
