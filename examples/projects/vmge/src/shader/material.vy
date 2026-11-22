use "compile.vy";

# Meta stride 5: stable ID, GLtype, arraylen, width, active in draft schema.
# Published programs resolve the full (ID,type,arraylen), including orphan rows.

# Independent material overrides keyed by stable parameter IDs, with active/orphan metadata and
# borrowed texture bindings. Persistent IDs are distinct from native texture tokens/session IDs.
#
# Fields:
#     state: Persistent document/instance IDs, live row count, and semantic edit revision.
#     meta: Per-row stable parameter IDs, typed shapes, and active/orphan status.
#     values: Independent typed numeric override components.
#     bindings: Stride four: parameter ID, resource ID, borrowed native texture token, session.
#     binding_state: Live sampler-reference count at index zero.
interface MaterialInstance {
    state :: Array<Int64>,
    meta :: Array<Int64>,
    values :: Array<Float64>,
    bindings :: Array<Int64>,
    binding_state :: Array<Int64>
}

# Allocate a material with explicit parameter-row and sampler-binding capacities.
#
# Parameters:
#     document_id (Int64): Stable shader-document ID in the registry; native tokens are not
#         serialized.
#     instance_id (Int64): Stable material-instance ID used in resource persistence.
#     row_capacity (Int64): Retained material parameter/orphan capacity, from 1 through 128 rows.
#     binding_capacity (Int64): Retained sampler-reference capacity, from 0 through 64.
#
# Returns:
#     MaterialInstance containing the resulting retained state or payload.
#
# Errors:
#     Invalid material resource ID/capacity.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_material_sized(
    document_id :: Int64,
    instance_id :: Int64,
    row_capacity :: Int64,
    binding_capacity :: Int64
) -> MaterialInstance {
    if document_id < 1
        || document_id > 1000000
        || instance_id < 1
        || instance_id > 1000000
        || row_capacity < 1
        || row_capacity > 128
        || binding_capacity < 0
        || binding_capacity > 64 {
        throw "Invalid material resource ID/capacity";
    }
    m :: MaterialInstance := MaterialInstance(
        ss_ints(4),
        ss_ints(row_capacity * 5),
        ss_floats(row_capacity * 64),
        ss_ints(binding_capacity * 4),
        ss_ints(1)
    );
    m.state[0] = document_id;
    m.state[1] = instance_id;
    return m;
}

# Create a material with default capacities of 128 parameter/orphan rows and 64 sampler references.
#
# Parameters:
#     document_id (Int64): Stable shader-document ID in the registry; native tokens are not
#         serialized.
#     instance_id (Int64): Stable material-instance ID used in resource persistence.
#
# Returns:
#     MaterialInstance containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_material(document_id :: Int64, instance_id :: Int64) -> MaterialInstance {
    return ss_material_sized(document_id, instance_id, 128, 64);
}

# Return the preallocated parameter/orphan row capacity of a material.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#
# Returns:
#     Preallocated parameter/orphan row capacity.
fn ss_material_rows(m :: MaterialInstance) -> Int64 {
    return m.meta.size() / 5;
}

# Return the preallocated sampler-binding capacity of a material.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#
# Returns:
#     Preallocated sampler-reference capacity.
fn ss_material_binding_capacity(m :: MaterialInstance) -> Int64 {
    return m.bindings.size() / 4;
}

# Create a temporary material candidate with the same row/binding capacities and persistent IDs as an
# existing instance.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#
# Returns:
#     MaterialInstance containing the resulting retained state or payload.
fn ss_material_like(m :: MaterialInstance) -> MaterialInstance {
    return ss_material_sized(m.state[0], m.state[1], ss_material_rows(m), ss_material_binding_capacity(m));
}

# Find an active material override by stable parameter ID; return -1 if absent.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#
# Returns:
#     The active material row, or -1 when absent.
fn ss_material_slot(m :: MaterialInstance, id :: Int64) -> Int64 {
    i :: Int64 := 0;
    fallback :: Int64 := -1;
    while i < m.state[2] {
        if m.meta[i * 5] == id {
            if m.meta[i * 5 + 4] == 1 {
                return i;
            }
            fallback = i;
        }
        i = i + 1;
    }
    return fallback;
}

# Find the retained row matching stable ID, GL type, and array length, including published-schema
# orphan overrides.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     gltype (Int64): OpenGL uniform type enum used for reflection and typed validation.
#     arraylen (Int64): Uniform array length; component width must fit the schema row.
#
# Returns:
#     Compatible active/orphan row, or -1 when the typed stable-ID override is absent.
fn ss_material_compatible_slot(
    m :: MaterialInstance,
    id :: Int64,
    gltype :: Int64,
    arraylen :: Int64
) -> Int64 {
    i :: Int64 := 0;
    while i < m.state[2] {
        if m.meta[i * 5] == id && m.meta[i * 5 + 1] == gltype && m.meta[i * 5 + 2] == arraylen {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

# Find a sampler binding by stable parameter ID; return -1 if absent.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#
# Returns:
#     The binding slot, or -1 when absent.
fn ss_material_binding(m :: MaterialInstance, id :: Int64) -> Int64 {
    i :: Int64 := 0;
    while i < m.binding_state[0] {
        if m.bindings[i * 4] == id {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

# Validate a material's counts, parameter shapes/values, stable IDs, and resource-binding records.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Invalid material storage/count.
#     Invalid material component schema.
#     Duplicate material compatible/active row.
#     Invalid material sampler resource ID.
#     Duplicate material sampler binding.
fn ss_material_validate(m :: MaterialInstance) -> Bool {
    rows :: Int64 := ss_material_rows(m);
    if rows < 1
        || rows > 128
        || m.meta.size() != rows * 5
        || m.values.size() != rows * 64
        || m.state[2] < 0
        || m.state[2] > rows
        || m.binding_state[0] < 0
        || m.binding_state[0] > ss_material_binding_capacity(m) {
        throw "Invalid material storage/count";
    }
    i :: Int64 := 0;
    while i < m.state[2] {
        gl :: Int64 := m.meta[i * 5 + 1];
        width :: Int64 := m.meta[i * 5 + 3];
        if m.meta[i * 5] < 1
            || m.meta[i * 5] > 1000000
            || m.meta[i * 5 + 2] < 1
            || width < 1
            || width > 64
            || width != ss_components(gl) * m.meta[i * 5 + 2]
            || m.meta[i * 5 + 4] < 0
            || m.meta[i * 5 + 4] > 1 {
            throw "Invalid material component schema";
        }
        j :: Int64 := 0;
        while j < width {
            ss_component_validate(gl, m.values[i * 64 + j]);
            j = j + 1;
        }
        j = 0;
        while j < i {
            if m.meta[j * 5] == m.meta[i * 5]
                && ((m.meta[j * 5 + 1] == gl && m.meta[j * 5 + 2] == m.meta[i * 5 + 2]) || (m.meta[j * 5 + 4] == 1 && m.meta[i * 5 + 4] == 1)) {
                throw "Duplicate material compatible/active row";
            }
            j = j + 1;
        }
        i = i + 1;
    }
    i = 0;
    while i < m.binding_state[0] {
        if m.bindings[i * 4] < 1 || m.bindings[i * 4 + 1] < 1 || m.bindings[i * 4 + 1] > 1000000 {
            throw "Invalid material sampler resource ID";
        }
        j :: Int64 := 0;
        while j < i {
            if m.bindings[j * 4] == m.bindings[i * 4] {
                throw "Duplicate material sampler binding";
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return true;
}

# Preflight destination capacity, then copy all live material rows/bindings and clear unused storage.
#
# Parameters:
#     to (MaterialInstance): Existing destination owner; its preallocated capacity must fit the copied
#         payload.
#     from (MaterialInstance): Source owner whose live payload is copied.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Material copy destination capacity.
fn ss_material_copy(to :: MaterialInstance, from :: MaterialInstance) -> Bool {
    ss_material_validate(from);
    if from.state[2] > ss_material_rows(to)
        || from.binding_state[0] > ss_material_binding_capacity(to)
        || to.values.size() != ss_material_rows(to) * 64 {
        throw "Material copy destination capacity";
    }
    live_meta :: Int64 := from.state[2] * 5;
    live_values :: Int64 := from.state[2] * 64;
    live_bindings :: Int64 := from.binding_state[0] * 4;
    i :: Int64 := 0;
    while i < live_meta {
        to.meta[i] = from.meta[i];
        i = i + 1;
    }
    while i < to.meta.size() {
        to.meta[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < live_values {
        to.values[i] = from.values[i];
        i = i + 1;
    }
    while i < to.values.size() {
        to.values[i] = 0.0;
        i = i + 1;
    }
    i = 0;
    while i < live_bindings {
        to.bindings[i] = from.bindings[i];
        i = i + 1;
    }
    while i < to.bindings.size() {
        to.bindings[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < 4 {
        to.state[i] = from.state[i];
        i = i + 1;
    }
    to.binding_state[0] = from.binding_state[0];
    return true;
}

# Synchronize material rows with a document schema, preserving compatible overrides and retaining
# missing/incompatible rows as orphans.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Retained material override outside draft schema range.
#     Material migration orphan capacity.
fn ss_material_migrate(m :: MaterialInstance, d :: SSDocument) -> Bool {
    ss_material_validate(m);
    p :: SSParameters := d.parameters;
    ss_parameters_validate(p);
    missing :: Int64 := 0;
    i :: Int64 := 0;
    # Validate every compatible retained override and total capacity before editing.
    while i < p.state[0] {
        s :: Int64 := ss_material_compatible_slot(m, p.meta[i * 8], p.meta[i * 8 + 1], p.meta[i * 8 + 2]);
        if s < 0 {
            missing = missing + 1;
        } else {
            j :: Int64 := 0;
            while j < p.meta[i * 8 + 3] {
                v :: Float64 := m.values[s * 64 + j];
                if v < p.ranges[i * 2] || v > p.ranges[i * 2 + 1] {
                    throw "Retained material override outside draft schema range";
                }
                j = j + 1;
            }
        }
        i = i + 1;
    }
    if missing + m.state[2] > ss_material_rows(m) {
        throw "Material migration orphan capacity";
    }
    i = 0;
    while i < m.state[2] {
        m.meta[i * 5 + 4] = 0;
        i = i + 1;
    }
    i = 0;
    while i < p.state[0] {
        s :: Int64 := ss_material_compatible_slot(m, p.meta[i * 8], p.meta[i * 8 + 1], p.meta[i * 8 + 2]);
        if s < 0 {
            s = m.state[2];
            m.state[2] = s + 1;
            m.meta[s * 5] = p.meta[i * 8];
            m.meta[s * 5 + 1] = p.meta[i * 8 + 1];
            m.meta[s * 5 + 2] = p.meta[i * 8 + 2];
            m.meta[s * 5 + 3] = p.meta[i * 8 + 3];
            j :: Int64 := 0;
            while j < p.meta[i * 8 + 3] {
                m.values[s * 64 + j] = p.values[i * 64 + j];
                j = j + 1;
            }
        }
        m.meta[s * 5 + 4] = 1;
        i = i + 1;
    }
    m.state[3] = m.state[3] + 1;
    return true;
}

# Validate and update one material component selected by stable parameter ID.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
#
# Errors:
#     Invalid material override component.
fn ss_material_set(m :: MaterialInstance, id :: Int64, component :: Int64, value :: Float64) -> Bool {
    s :: Int64 := ss_material_slot(m, id);
    if s < 0 || component < 0 || component >= m.meta[s * 5 + 3] {
        throw "Invalid material override component";
    }
    ss_component_validate(m.meta[s * 5 + 1], value);
    if m.values[s * 64 + component] == value {
        return false;
    }
    m.values[s * 64 + component] = value;
    m.state[3] = m.state[3] + 1;
    return true;
}

# Read one material component selected by stable parameter ID.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     component (Int64): Zero-based component within the selected parameter row.
#
# Returns:
#     The computed Float64 value: m.values[s * 64 + component].
#
# Errors:
#     Unknown material parameter.
fn ss_material_get(m :: MaterialInstance, id :: Int64, component :: Int64) -> Float64 {
    s :: Int64 := ss_material_slot(m, id);
    if s < 0 || component < 0 || component >= m.meta[s * 5 + 3] {
        throw "Unknown material parameter";
    }
    return m.values[s * 64 + component];
}

# Stage schema validation and material migration, then atomically publish the coupled
# document/material numeric edit.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     True when the coupled numeric edit changed its value; false when unchanged.
fn ss_material_parameter_set(
    d :: SSDocument,
    m :: MaterialInstance,
    id :: Int64,
    component :: Int64,
    value :: Float64
) -> Bool {
    ss_parameter_validate(d, id, component, value);
    changed :: Bool := false;
    @bounded<1048576> region material_parameter_edit {
        candidate :: MaterialInstance := ss_material_like(m);
        ss_material_copy(candidate, m);
        ss_material_migrate(candidate, d);
        if candidate.state[2] != m.state[2] {
            changed = true;
        }
        i :: Int64 := 0;
        while i < candidate.meta.size() {
            if candidate.meta[i] != m.meta[i] {
                changed = true;
            }
            i = i + 1;
        }
        if ss_material_set(candidate, id, component, value) {
            changed = true;
        }
        ss_material_copy(m, candidate);
        if ss_parameter_set(d, id, component, value) {
            changed = true;
        }
    };
    return changed;
}

# Resolve a scalar sampler parameter to a persistent resource ID and borrowed managed texture
# token/session.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     resource_id (Int64): Persistent texture-resource ID resolved separately from its native token.
#     texture (vraylib.Texture): Borrowed managed texture; the material does not take ownership.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Invalid material sampler binding.
#     Material sampler capacity.
fn ss_material_bind_texture(
    m :: MaterialInstance,
    id :: Int64,
    resource_id :: Int64,
    texture :: vraylib.Texture
) -> Bool {
    s :: Int64 := ss_material_compatible_slot(m, id, 35678, 1);
    if s < 0 || resource_id < 1 || resource_id > 1000000 || !texture.valid() {
        throw "Invalid material sampler binding";
    }
    b :: Int64 := ss_material_binding(m, id);
    if b < 0 {
        b = m.binding_state[0];
        if b >= ss_material_binding_capacity(m) {
            throw "Material sampler capacity";
        }
        m.binding_state[0] = b + 1;
    }
    m.bindings[b * 4] = id;
    m.bindings[b * 4 + 1] = resource_id;
    m.bindings[b * 4 + 2] = texture.token;
    m.bindings[b * 4 + 3] = texture.session;
    m.state[3] = m.state[3] + 1;
    return true;
}

# Resolve a material override compatible with the last-good published schema, falling back to the
# published default.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     row (Int64): Zero-based displayed/schema/diagnostic row index.
#     component (Int64): Zero-based component within the selected parameter row.
#
# Returns:
#     The Float64 value selected by the conditions described above.
fn ss_material_published_value(
    d :: SSDocument,
    m :: MaterialInstance,
    row :: Int64,
    component :: Int64
) -> Float64 {
    p :: SSParameters := d.published;
    s :: Int64 := ss_material_compatible_slot(m, p.meta[row * 8], p.meta[row * 8 + 1], p.meta[row * 8 + 2]);
    if s >= 0 {
        return m.values[s * 64 + component];
    }
    edit :: Int64 := ss_parameter_slot(d.parameters, p.meta[row * 8]);
    if edit >= 0 {
        if d.parameters.meta[edit * 8 + 1] == p.meta[row * 8 + 1]
            && d.parameters.meta[edit * 8 + 2] == p.meta[row * 8 + 2] {
            return d.parameters.values[edit * 64 + component];
        }
    }
    return p.values[row * 64 + component];
}

# Report whether a uniform name is one of the pipeline-owned inputTexture/inputTexture1 samplers.
#
# Parameters:
#     name (String): Display/resource/uniform name or environment-variable key, according to this API.
#
# Returns:
#     The Boolean result of name == "inputTexture" || name == "inputTexture1".
fn ss_controlled_sampler(name :: String) -> Bool {
    return name == "inputTexture" || name == "inputTexture1";
}

# Preflight a material's numeric components and sampler bindings against the last-good program schema
# before uploads.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     controlled (Bool): Whether pipeline-owned input samplers are supplied separately from material
#         bindings.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Material requires a compiled document.
#     Material sampler requires a resolved live texture binding.
#     Material override outside published schema range.
fn ss_material_validate_for(d :: SSDocument, m :: MaterialInstance, controlled :: Bool) -> Bool {
    ss_material_validate(m);
    p :: SSParameters := d.published;
    ss_parameters_validate(p);
    shader :: vraylib.Shader := ss_program(d);
    if !shader.valid() {
        throw "Material requires a compiled document";
    }
    i :: Int64 := 0;
    while i < p.state[0] {
        if p.meta[i * 8 + 6] == 1 {
            if p.meta[i * 8 + 1] == 35678 {
                if !controlled || !ss_controlled_sampler(se_text_read(p.names[i])) {
                    b :: Int64 := ss_material_binding(m, p.meta[i * 8]);
                    if b < 0 || p.meta[i * 8 + 2] != 1 || m.bindings[b * 4 + 3] != shader.session || !vraylib.Texture(
                        m.bindings[b * 4 + 2],
                        m.bindings[b * 4 + 3],
                        false
                    ).valid() {
                        throw "Material sampler requires a resolved live texture binding";
                    }
                }
            } else {
                j :: Int64 := 0;
                while j < p.meta[i * 8 + 3] {
                    v :: Float64 := ss_material_published_value(d, m, i, j);
                    ss_component_validate(p.meta[i * 8 + 1], v);
                    if v < p.ranges[i * 2] || v > p.ranges[i * 2 + 1] {
                        throw "Material override outside published schema range";
                    }
                    j = j + 1;
                }
            }
        }
        i = i + 1;
    }
    return true;
}

# Upload every published parameter and resolved sampler for an instance, optionally reserving
# controlled pipeline samplers.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     controlled (Bool): Whether pipeline-owned input samplers are supplied separately from material
#         bindings.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_material_apply_mode(d :: SSDocument, m :: MaterialInstance, controlled :: Bool) -> Bool {
    ss_material_validate_for(d, m, controlled);
    shader :: vraylib.Shader := ss_program(d);
    p :: SSParameters := d.published;
    @bounded<1048576> region material_uniforms {
        values :: Array<Float64> := [];
        values.reserve(64);
        i :: Int64 := 0;
        while i < p.state[0] {
            if p.meta[i * 8 + 6] == 1 {
                if p.meta[i * 8 + 1] == 35678 {
                    if !controlled || !ss_controlled_sampler(se_text_read(p.names[i])) {
                        b :: Int64 := ss_material_binding(m, p.meta[i * 8]);
                        shader.set_texture(
                            se_text_read(p.names[i]),
                            vraylib.Texture(m.bindings[b * 4 + 2], m.bindings[b * 4 + 3], false)
                        );
                    }
                } else {
                    values.resize(p.meta[i * 8 + 3], 0.0);
                    j :: Int64 := 0;
                    while j < p.meta[i * 8 + 3] {
                        values[j] = ss_material_published_value(d, m, i, j);
                        j = j + 1;
                    }
                    shader.set_typed(se_text_read(p.names[i]), p.meta[i * 8 + 1], values);
                }
            }
            i = i + 1;
        }
    };
    d.state[3] = d.state[2];
    d.state[9] = d.state[8];
    return true;
}

# Reset all reflected instance values and sampler bindings before a material draw.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#
# Returns:
#     The Boolean result of ss_material_apply_mode(d, m, false).
fn ss_material_apply(d :: SSDocument, m :: MaterialInstance) -> Bool {
    return ss_material_apply_mode(d, m, false);
}

# Draw shared model geometry at a transform using per-instance values without persistently changing
# the model's material state.
#
# Parameters:
#     model (vraylib.Model): Managed model whose geometry is borrowed for this draw.
#     transform (vraylib.Matrix): Model/world transform applied to the shared geometry.
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#     tint (vraylib.Color): RGBA tint applied while drawing.
#
# Returns:
#     The Boolean result of model.draw_material(transform, ss_program(d), slot, tint).
#
# Lifecycle:
#     Use inside the balanced model/camera drawing path. Model geometry is shared; material values are
#     uploaded for each draw and sampler textures remain borrowed.
fn ss_material_draw(
    model :: vraylib.Model,
    transform :: vraylib.Matrix,
    d :: SSDocument,
    m :: MaterialInstance,
    slot :: Int64,
    tint :: vraylib.Color
) -> Bool {
    ss_material_apply(d, m);
    return model.draw_material(transform, ss_program(d), slot, tint);
}

# Serialize material IDs, exact parameter/orphan values, and persistent sampler-resource IDs without
# native tokens.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_material_text(m :: MaterialInstance) -> String {
    ss_material_validate(m);
    b :: Int64 := vcore.sb_create();
    vcore.sb_append(b, "VMGE_MATERIAL|1|" + string(m.state[0]) + "|" + string(m.state[1]) + "\n");
    i :: Int64 := 0;
    while i < m.state[2] {
        vcore.sb_append(b, "P");
        j :: Int64 := 0;
        while j < 5 {
            vcore.sb_append(b, "|" + string(m.meta[i * 5 + j]));
            j = j + 1;
        }
        j = 0;
        while j < m.meta[i * 5 + 3] {
            vcore.sb_append(b, "|" + ss_encode(m.values[i * 64 + j]));
            j = j + 1;
        }
        vcore.sb_append(b, "\n");
        i = i + 1;
    }
    i = 0;
    while i < m.binding_state[0] {
        vcore.sb_append(b, "T|" + string(m.bindings[i * 4]) + "|" + string(m.bindings[i * 4 + 1]) + "\n");
        i = i + 1;
    }
    return vcore.sb_build(b);
}

# Validate material-resource text into candidate storage before publishing retained instance
# rows/bindings.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Material resource budget.
#     Unsupported material version.
#     Invalid material IDs.
#     Invalid material sampler reference/capacity.
#     Invalid material record/capacity.
#     Invalid material record width.
fn ss_material_parse(m :: MaterialInstance, text :: String) -> Bool {
    if text.size() > 262144 {
        throw "Material resource budget";
    }
    candidate :: MaterialInstance := ss_material_like(m);
    lines :: Array := ss_split(text, "\n");
    header :: Array := ss_split(lines[0], "|");
    if header.size() != 4 || header[0] != "VMGE_MATERIAL" || header[1] != "1" {
        throw "Unsupported material version";
    }
    candidate.state[0] = ss_integer(header[2]);
    candidate.state[1] = ss_integer(header[3]);
    if candidate.state[0] < 1
        || candidate.state[0] > 1000000
        || candidate.state[1] < 1
        || candidate.state[1] > 1000000 {
        throw "Invalid material IDs";
    }
    row :: Int64 := 1;
    while row < lines.size() {
        if lines[row] != "" {
            f :: Array := ss_split(lines[row], "|");
            if f[0] == "T" {
                b :: Int64 := candidate.binding_state[0];
                if f.size() != 3 || b >= ss_material_binding_capacity(candidate) {
                    throw "Invalid material sampler reference/capacity";
                }
                candidate.bindings[b * 4] = ss_integer(f[1]);
                candidate.bindings[b * 4 + 1] = ss_integer(f[2]);
                candidate.binding_state[0] = b + 1;
            } else {
                s :: Int64 := candidate.state[2];
                if s >= ss_material_rows(candidate) || f.size() < 7 || f[0] != "P" {
                    throw "Invalid material record/capacity";
                }
                j :: Int64 := 0;
                while j < 5 {
                    candidate.meta[s * 5 + j] = ss_integer(f[j + 1]);
                    j = j + 1;
                }
                width :: Int64 := candidate.meta[s * 5 + 3];
                if width < 1 || width > 64 || f.size() != 6 + width {
                    throw "Invalid material record width";
                }
                j = 0;
                while j < width {
                    value :: Float64 := ss_number(f[6 + j]);
                    ss_component_validate(candidate.meta[s * 5 + 1], value);
                    candidate.values[s * 64 + j] = value;
                    j = j + 1;
                }
                candidate.state[2] = s + 1;
            }
        }
        row = row + 1;
    }
    ss_material_copy(m, candidate);
    return true;
}

# Atomically save the standalone material resource.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Cannot save material.
fn ss_material_save(m :: MaterialInstance, path :: String) -> Bool {
    @bounded<16777216> region material_save {
        if !vfs.write_atomic(path, ss_material_text(m)) {
            throw "Cannot save material";
        }
    };
    return true;
}

# Read and validate the standalone material resource; native textures must be resolved separately
# after reopening.
#
# Parameters:
#     m (MaterialInstance): Independent numeric overrides and borrowed sampler-resource bindings for
#         one material instance.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Cannot read material.
fn ss_material_load(m :: MaterialInstance, path :: String) -> Bool {
    @bounded<33554432> region material_load {
        text := vfs.read(path);
        if text == null {
            throw "Cannot read material";
        }
        ss_material_parse(m, text);
    };
    return true;
}
