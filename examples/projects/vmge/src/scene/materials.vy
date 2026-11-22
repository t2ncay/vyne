use "../shader/material.vy";

# Host limits: 128 nodes, 16 overrides (64 components each), 16 samplers,
# 255-byte stable shader paths. All banks are constructed before frames.

# Retained per-node material instances, shader paths, and a bounded persistent texture-resource table,
# separate from native asset caches.
#
# Fields:
#     instances: 128 separate material-instance owners, one per scene slot.
#     paths: 128 retained shader-publication references, one per scene slot.
#     resources: 64 retained persistent texture-resource paths.
#     resource_state: Live texture-resource count at index zero.
interface SEMaterials {
    instances :: Array,
    paths :: Array,
    resources :: Array,
    resource_state :: Array<Int64>,
}

# Preallocated material-history payload containing instance values, metadata, bindings, shader paths,
# and resource references.
#
# Fields:
#     integers: Packed per-instance metadata and persistent binding records.
#     numbers: Packed per-instance numeric parameter/orphan values.
#     paths: Packed length-prefixed shader references for every node.
#     resources: Packed retained texture-resource paths.
#     state: Mutable numeric state/counters used by this operation.
interface SEMaterialSnapshot {
    integers :: Array<Int64>,
    numbers :: Array<Float64>,
    paths :: Array<Int64>,
    resources :: Array<Int64>,
    state :: Array<Int64>,
}

# Bounded shared shader-document bank retaining immutable scene/pass publications and canonical cache
# paths.
#
# Fields:
#     documents: Eight preallocated publication owners plus editable/identity registry slots.
#     paths: Canonical retained program cache paths.
#     state: Live bounded program-cache count.
interface SEPrograms {
    documents :: Array,
    paths :: Array,
    state :: Array<Int64>,
}

# Create eight publication-document owners and two registry slots reserved for editable/identity
# programs.
#
# Returns:
#     SEPrograms containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn se_programs() -> SEPrograms {
    # One length prefix + 65,535 source bytes fits a 65,536-element buffer.
    # Avoid doubling retained program sources to 131,072 elements; the
    # editable studio document keeps its existing full 64 KiB source limit.
    docs :: Array := [];
    docs.reserve(10);
    i :: Int64 := 0;
    while i < 8 {
        docs.push(ss_document_with_limits(65535, 2048, 64));
        i = i + 1;
    }
    docs.push(null);
    docs.push(null);
    paths :: Array := [
        ss_text(2048),
        ss_text(2048),
        ss_text(2048),
        ss_text(2048),
        ss_text(2048),
        ss_text(2048),
        ss_text(2048),
        ss_text(2048)
    ];
    return SEPrograms(docs, paths, ss_ints(1));
}

# Resolve or compile a shader publication in the bounded program bank, verifying content-addressed
# resources.
#
# Parameters:
#     bank (SEPrograms): Asset/program cache associated with the application session.
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     The Int64 value selected by the conditions described above.
#
# Errors:
#     Scene shader registry capacity: 8 published resources (restart to reclaim history).
#     Shader publication content does not match its immutable resource hash.
#     Scene material compile failed (with contextual detail).
fn se_program_resolve(bank :: SEPrograms, app :: vraylib.App, path :: String) -> Int64 {
    full :: String := ss_normal_path(path);
    i :: Int64 := 0;
    while i < bank.state[0] {
        if ss_normal_path(se_text_read(bank.paths[i])) == full {
            return i;
        }
        i = i + 1;
    }
    if i == 8 {
        throw "Scene shader registry capacity: 8 published resources (restart to reclaim history)";
    }
    d :: SSDocument := bank.documents[i];
    ss_document_load(d, full);
    if se_material_publication(path) {
        if vfs.basename(full) != "host-" + string(d.state[7]) + ".vshader" {
            throw "Shader publication content does not match its immutable resource hash";
        }
    }
    if !ss_compile(app, d) {
        throw "Scene material compile failed: " + se_text_read(d.log);
    }
    se_text_write(bank.paths[i], full);
    bank.state[0] = i + 1;
    return i;
}

# Close and collect all retained shader programs before the owning application's assets/window close.
#
# Parameters:
#     bank (SEPrograms): Asset/program cache associated with the application session.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Scene shader dependencies remain at shutdown.
fn se_programs_close(bank :: SEPrograms) -> Bool {
    i :: Int64 := 0;
    while i < 8 {
        d :: SSDocument := bank.documents[i];
        ss_document_close(d);
        if !ss_document_collect(d) {
            throw "Scene shader dependencies remain at shutdown";
        }
        i = i + 1;
    }
    return true;
}

# Allocate independent material instances and retained shader/resource paths for every scene slot
# before frames.
#
# Returns:
#     SEMaterials containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn se_materials() -> SEMaterials {
    instances :: Array := [];
    paths :: Array := [];
    instances.reserve(128);
    paths.reserve(128);
    i :: Int64 := 0;
    while i < 128 {
        instances.push(ss_material_sized(1, i + 1, 16, 16));
        paths.push(ss_text(255));
        i = i + 1;
    }
    resources :: Array := [];
    resources.reserve(64);
    i = 0;
    while i < 64 {
        resources.push(ss_text(255));
        i = i + 1;
    }
    return SEMaterials(instances, paths, resources, ss_ints(1));
}

# Validate that a material fits the host's sixteen parameter/orphan rows and sixteen sampler-reference
# slots.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Scene material limit: 16 parameters / 16 samplers.
fn se_material_limit(m :: MaterialInstance) -> Bool {
    if m.state[2] > 16 || m.binding_state[0] > 16 {
        throw "Scene material limit: 16 parameters / 16 samplers";
    }
    return true;
}

# Copy material payloads through the engine's capacity-checked instance copy helper.
#
# Parameters:
#     to (MaterialInstance): Existing destination owner; its preallocated capacity must fit the copied
#         payload.
#     from (MaterialInstance): Source owner whose live payload is copied.
#
# Returns:
#     The Boolean result of ss_material_copy(to, from).
fn se_material_copy(to :: MaterialInstance, from :: MaterialInstance) -> Bool {
    return ss_material_copy(to, from);
}

# Copy one scene slot's material instance and shader reference into another existing slot owner.
#
# Parameters:
#     to (SEMaterials): Existing destination owner; its preallocated capacity must fit the copied
#         payload.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#     from (SEMaterials): Source owner whose live payload is copied.
#     source (Int64): Source owner whose payload is read or copied by this operation.
#
# Returns:
#     True when the operation completes without throwing.
fn se_material_node_copy(to :: SEMaterials, slot :: Int64, from :: SEMaterials, source :: Int64) -> Bool {
    se_material_limit(from.instances[source]);
    se_material_copy(to.instances[slot], from.instances[source]);
    m :: MaterialInstance := to.instances[slot];
    m.state[1] = slot + 1;
    se_text_copy(to.paths[slot], from.paths[source]);
    return true;
}

# Report whether a path identifies a content-addressed immutable host shader publication.
#
# Parameters:
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when a path identifies a content-addressed immutable host shader publication; false
#     otherwise.
fn se_material_publication(path :: String) -> Bool {
    name :: String := vfs.basename(path);
    if name.size() < 14 || vfs.extension(name) != ".vshader" {
        return false;
    }
    return vraylib.text_subtext(name, 0, 5) == "host-";
}

# Reject source writes that would change an immutable publication's existing content.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Cannot save to a content-addressed publication path.
#     Immutable shader publication: open an editable studio copy before saving.
fn se_material_guard_source(d :: SSDocument, path :: String) -> Bool {
    if !se_material_publication(path) {
        return true;
    }
    raw := vfs.read(path);
    if raw == null {
        throw "Cannot save to a content-addressed publication path";
    }
    if raw != ss_document_text(d) {
        throw "Immutable shader publication: open an editable studio copy before saving";
    }
    return true;
}

# Clear scene material assignments and resource-table state in existing owners.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     document_id (Int64): Stable shader-document ID in the registry; native tokens are not
#         serialized.
#     instance_id (Int64): Stable material-instance ID used in resource persistence.
#
# Returns:
#     True when the operation completes without throwing.
fn se_material_reset(m :: MaterialInstance, document_id :: Int64, instance_id :: Int64) -> Bool {
    i :: Int64 := 0;
    while i < m.state.size() {
        m.state[i] = 0;
        i = i + 1;
    }
    m.state[0] = document_id;
    m.state[1] = instance_id;
    i = 0;
    while i < m.meta.size() {
        m.meta[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < m.values.size() {
        m.values[i] = 0.0;
        i = i + 1;
    }
    i = 0;
    while i < m.bindings.size() {
        m.bindings[i] = 0;
        i = i + 1;
    }
    m.binding_state[0] = 0;
    return true;
}

# Separate power-of-two path/resource sections avoid rounding 68,225 Int64s
# up to 131,072 per snapshot. Numeric values remain an exact power-of-two bank.

# Allocate the complete fixed material-history snapshot layout before managed frames.
#
# Returns:
#     SEMaterialSnapshot containing the resulting retained state or payload.
fn se_material_snapshot() -> SEMaterialSnapshot {
    return SEMaterialSnapshot(
        ss_ints(128 * 149),
        ss_floats(128 * 1024),
        ss_ints(128 * 256),
        ss_ints(64 * 256),
        ss_ints(1)
    );
}

# Capture or restore scene material instances, shader references, and texture-resource paths using an
# existing snapshot.
#
# Parameters:
#     snapshot (SEMaterialSnapshot): Preacquired material-history snapshot used to capture or restore
#         instance/resource data.
#     bank (SEMaterials): Scene material instances, shader references, and persistent texture-resource
#         paths.
#     restore (Bool): Whether to copy the snapshot back into its live owner.
#
# Returns:
#     True when the operation completes without throwing.
fn se_material_capture(snapshot :: SEMaterialSnapshot, bank :: SEMaterials, restore :: Bool) -> Bool {
    slot :: Int64 := 0;
    while slot < 128 {
        m :: MaterialInstance := bank.instances[slot];
        path :: Array<Int64> := bank.paths[slot];
        b :: Int64 := slot * 149;
        if !restore {
            se_material_limit(m);
        }
        i :: Int64 := 0;
        while i < 4 {
            if restore {
                m.state[i] = snapshot.integers[b + i];
            } else {
                snapshot.integers[b + i] = m.state[i];
            }
            i = i + 1;
        }
        i = 0;
        while i < 80 {
            if restore {
                m.meta[i] = snapshot.integers[b + 4 + i];
            } else {
                snapshot.integers[b + 4 + i] = m.meta[i];
            }
            i = i + 1;
        }
        i = 0;
        while i < 64 {
            if restore {
                m.bindings[i] = snapshot.integers[b + 84 + i];
            } else {
                snapshot.integers[b + 84 + i] = m.bindings[i];
            }
            i = i + 1;
        }
        if restore {
            m.binding_state[0] = snapshot.integers[b + 148];
        } else {
            snapshot.integers[b + 148] = m.binding_state[0];
        }
        i = 0;
        while i < 256 {
            if restore {
                path[i] = snapshot.paths[slot * 256 + i];
            } else {
                snapshot.paths[slot * 256 + i] = path[i];
            }
            i = i + 1;
        }
        i = 0;
        while i < 1024 {
            if restore {
                m.values[i] = snapshot.numbers[slot * 1024 + i];
            } else {
                snapshot.numbers[slot * 1024 + i] = m.values[i];
            }
            i = i + 1;
        }
        slot = slot + 1;
    }
    if restore {
        bank.resource_state[0] = snapshot.state[0];
    } else {
        snapshot.state[0] = bank.resource_state[0];
    }
    slot = 0;
    while slot < 64 {
        path :: Array<Int64> := bank.resources[slot];
        i :: Int64 := 0;
        while i < 256 {
            if restore {
                path[i] = snapshot.resources[slot * 256 + i];
            } else {
                snapshot.resources[slot * 256 + i] = path[i];
            }
            i = i + 1;
        }
        slot = slot + 1;
    }
    return true;
}

# Find or append a retained texture-resource path and return its persistent resource ID.
#
# Parameters:
#     bank (SEMaterials): Scene material instances, shader references, and persistent texture-resource
#         paths.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     The Int64 value selected by the conditions described above.
#
# Errors:
#     Texture resource path exceeds 255 bytes.
#     Scene texture resource capacity: 64.
fn se_material_resource(bank :: SEMaterials, path :: String) -> Int64 {
    if path.size() > 255 {
        throw "Texture resource path exceeds 255 bytes";
    }
    i :: Int64 := 0;
    while i < bank.resource_state[0] {
        if se_text_read(bank.resources[i]) == path {
            return i + 100;
        }
        i = i + 1;
    }
    if i == 64 {
        throw "Scene texture resource capacity: 64";
    }
    se_text_write(bank.resources[i], path);
    bank.resource_state[0] = i + 1;
    return i + 100;
}
