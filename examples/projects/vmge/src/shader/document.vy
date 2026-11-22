use "common.vy";

# Parameter meta stride 8: id, GLtype, arraylen, width, continuous, fit, reflection status, location.
# Status: 0 uncompiled, 1 active, 2 missing, 3 incompatible. Values/default stride 64.

# Fixed typed parameter schema. Metadata stride eight stores ID/type/array length/component
# width/continuous/fit/status/location; value/default rows have stride 64 and range rows stride two.
#
# Fields:
#     meta: Stride eight: stable ID, GL type, array length, component count, continuous, fit, status,
#         location.
#     values: Current numeric components at stride 64 per parameter row.
#     defaults: Default numeric components at stride 64 per parameter row.
#     ranges: Inclusive low/high numeric limits at stride two per row.
#     names: Preallocated retained name buffers.
#     groups: Retained UI group labels per parameter.
#     units: Parameter units label for display.
#     state: Live parameter count at index zero.
interface SSParameters {
    meta :: Array<Int64>,
    values :: Array<Float64>,
    defaults :: Array<Float64>,
    ranges :: Array<Float64>,
    names :: Array,
    groups :: Array,
    units :: Array,
    state :: Array<Int64>
}

# Retained editable shader source, paths, schema, revisions, include dependencies, diagnostics, last-
# good published schema, and deferred native-program releases. Tokens are session-local and never
# persisted.
#
# Fields:
#     source: Length-prefixed retained fragment GLSL source.
#     vertex: Length-prefixed retained vertex GLSL source; empty selects the default vertex shader.
#     path: Retained editable shader-resource filesystem path.
#     fragment_path: Effective fragment include-root path, falling back to the document path when
#         empty.
#     vertex_path: Effective vertex include-root path, falling back to the document path when empty.
#     state: Edit/disk/build/Apply/parameter revisions, program/session tokens, conflicts, hashes, and
#         release count.
#     parameters: Editable typed schema/default/current values, separate from last-good published
#         bindings.
#     log: Retained UTF-8 compiler/editor diagnostic text.
#     dependencies: Canonical retained paths for up to 32 successful-build include dependencies.
#     dependency_hashes: Observed successful-build include hashes used to detect external changes.
#     published: Last-good reflected schema kept separate from the editable draft.
#     retired: Up to sixteen native program tokens whose releases are deferred by dependencies.
#     retired_sessions: Application-session IDs parallel to retired program tokens.
interface SSDocument {
    source :: Array<Int64>,
    vertex :: Array<Int64>,
    path :: Array<Int64>,
    fragment_path :: Array<Int64>,
    vertex_path :: Array<Int64>,
    state :: Array<Int64>,
    parameters :: SSParameters,
    log :: Array<Int64>,
    dependencies :: Array,
    dependency_hashes :: Array<Int64>,
    published :: SSParameters,
    retired :: Array<Int64>,
    retired_sessions :: Array<Int64>
}

# Create parameter-schema storage with the default capacity of 64 rows.
#
# Returns:
#     An empty retained schema with capacity for 64 rows.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_parameters() -> SSParameters {
    return ss_parameters_sized(64);
}

# Create bounded parameter-schema storage with a caller-selected row capacity from 1 through 64.
#
# Parameters:
#     capacity (Int64): Maximum parameter rows, from 1 through 64.
#
# Returns:
#     An empty retained schema with the requested row capacity.
#
# Errors:
#     Shader parameter capacity must be 1 through 64.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_parameters_sized(capacity :: Int64) -> SSParameters {
    if capacity < 1 || capacity > 64 {
        throw "Shader parameter capacity must be 1 through 64";
    }
    names :: Array := [];
    groups :: Array := [];
    units :: Array := [];
    i :: Int64 := 0;
    while i < capacity {
        names.push(ss_text(127));
        groups.push(ss_text(127));
        units.push(ss_text(63));
        i = i + 1;
    }
    return SSParameters(
        ss_ints(capacity * 8),
        ss_floats(capacity * 64),
        ss_floats(capacity * 64),
        ss_floats(capacity * 2),
        names,
        groups,
        units,
        ss_ints(1)
    );
}

# Create a shader document with 64-KiB source stages, 2,048-byte paths, and 64 parameter rows.
#
# Returns:
#     A new shader document initialized with the default gain source/schema.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_document() -> SSDocument {
    return ss_document_with_limits(65536, 2048, 64);
}

# Create a bounded shader document with caller-selected source/dependency/schema capacities and the
# default exposed gain shader.
#
# Parameters:
#     source_capacity (Int64): Retained bytes per GLSL stage, from 256 through 65,536.
#     dependency_path_capacity (Int64): Retained bytes per include path, from 255 through 2,048.
#     parameter_capacity (Int64): Retained shader schema capacity, from 1 through 64 rows.
#
# Returns:
#     A new bounded shader document initialized with the default gain source/schema.
#
# Errors:
#     Invalid retained shader document capacities.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_document_with_limits(
    source_capacity :: Int64,
    dependency_path_capacity :: Int64,
    parameter_capacity :: Int64
) -> SSDocument {
    if source_capacity < 256
        || source_capacity > 65536
        || dependency_path_capacity < 255
        || dependency_path_capacity > 2048 {
        throw "Invalid retained shader document capacities";
    }
    deps :: Array := [];
    i :: Int64 := 0;
    while i < 32 {
        deps.push(ss_text(dependency_path_capacity));
        i = i + 1;
    }
    d :: SSDocument := SSDocument(
        ss_text(source_capacity),
        ss_text(source_capacity),
        ss_text(2048),
        ss_text(2048),
        ss_text(2048),
        ss_ints(16),
        ss_parameters_sized(parameter_capacity),
        ss_text(4095),
        deps,
        ss_ints(32),
        ss_parameters_sized(parameter_capacity),
        ss_ints(16),
        ss_ints(16)
    );
    d.state[0] = 1;
    d.state[11] = 1;
    se_text_write(
        d.source,
        "#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nuniform float gain;\nvoid main()\{finalColor=vec4(vec3(gain),1.0);}\n"
    );
    ss_parameter_define(d, 1, "gain", 5126, 1, 0.5, 0.0, 1.0, "Surface", "linear", true, true);
    return d;
}

# Advance edit and build revisions after source or compilation-relevant schema changes.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_document_source_change(d :: SSDocument) -> Bool {
    d.state[0] = d.state[0] + 1;
    d.state[11] = d.state[11] + 1;
    return true;
}

# Report whether the editable shader revision differs from its saved disk revision.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     The Boolean result of d.state[0] != d.state[1].
fn ss_document_dirty(d :: SSDocument) -> Bool {
    return d.state[0] != d.state[1];
}

# Report whether the source/schema build revision differs from the last successful compilation.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     The Boolean result of d.state[11] != d.state[2].
fn ss_document_compile_dirty(d :: SSDocument) -> Bool {
    return d.state[11] != d.state[2];
}

# Report whether current compiled source and parameter revisions match the last Apply state.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     The Boolean result of !ss_document_compile_dirty(d) && d.state[3] == d.state[2] && d.state[2] !=
#     0 && d.state[9] == d.state[8].
fn ss_document_applied(d :: SSDocument) -> Bool {
    return !ss_document_compile_dirty(d)
        && d.state[3] == d.state[2]
        && d.state[2] != 0
        && d.state[9] == d.state[8];
}

# Store driver/editor diagnostics in retained bytes, truncating oversized logs with a visible marker.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_document_log(d :: SSDocument, text :: String) -> Bool {
    if text.size() <= 3500 {
        return se_text_write(d.log, text);
    }
    b :: Int64 := vcore.sb_create();
    i :: Int64 := 0;
    while i < 3500 {
        vcore.sb_append(b, text[i]);
        i = i + 1;
    }
    vcore.sb_append(b, "\n[diagnostic truncated]");
    return se_text_write(d.log, vcore.sb_build(b));
}

# Find a schema row by stable parameter ID; return -1 if absent.
#
# Parameters:
#     p (SSParameters): Typed parameter-schema rows, component values/defaults, ranges, and retained
#         labels.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#
# Returns:
#     The zero-based schema row, or -1 when the stable ID is absent.
fn ss_parameter_slot(p :: SSParameters, id :: Int64) -> Int64 {
    i :: Int64 := 0;
    while i < p.state[0] {
        if p.meta[i * 8] == id {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

# Define a unique typed uniform row, broadcast its default across components, retain
# range/group/units, and advance schema revisions.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     name (String): Display/resource/uniform name or environment-variable key, according to this API.
#     gltype (Int64): OpenGL uniform type enum used for reflection and typed validation.
#     arraylen (Int64): Uniform array length; component width must fit the schema row.
#     def (Float64): Default numeric value broadcast across parameter components.
#     low (Float64): Inclusive lower limit.
#     high (Float64): Inclusive upper limit.
#     group_name (String): UI group label stored with the parameter schema.
#     units (String): Parameter units label for display.
#     continuous (Bool): Whether the parameter is eligible for continuous numeric adjustment.
#     fit (Bool): Whether the continuous scalar parameter is eligible for fitting.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Invalid shader parameter schema.
#     Duplicate shader parameter ID/name.
#     Shader parameter capacity.
fn ss_parameter_define(
    d :: SSDocument,
    id :: Int64,
    name :: String,
    gltype :: Int64,
    arraylen :: Int64,
    def :: Float64,
    low :: Float64,
    high :: Float64,
    group_name :: String,
    units :: String,
    continuous :: Bool,
    fit :: Bool
) -> Bool {
    p :: SSParameters := d.parameters;
    width :: Int64 := ss_components(gltype) * arraylen;
    if id < 1
        || id > 1000000
        || !ss_identifier(name)
        || arraylen < 1
        || width < 1
        || width > 64
        || !(low <= def && def <= high)
        || (continuous && !ss_continuous(gltype))
        || (fit && !continuous) {
        throw "Invalid shader parameter schema";
    }
    ss_range_validate(gltype, low, high);
    ss_component_validate(gltype, def);
    i :: Int64 := 0;
    while i < p.state[0] {
        if p.meta[i * 8] == id || se_text_read(p.names[i]) == name {
            throw "Duplicate shader parameter ID/name";
        }
        i = i + 1;
    }
    if i >= p.names.size() || group_name.size() > 127 || units.size() > 63 {
        throw "Shader parameter capacity";
    }
    se_text_write(p.names[i], name);
    se_text_write(p.groups[i], group_name);
    se_text_write(p.units[i], units);
    p.meta[i * 8] = id;
    p.meta[i * 8 + 1] = gltype;
    p.meta[i * 8 + 2] = arraylen;
    p.meta[i * 8 + 3] = width;
    p.meta[i * 8 + 4] = int64(continuous);
    p.meta[i * 8 + 5] = int64(fit);
    p.meta[i * 8 + 6] = 0;
    p.meta[i * 8 + 7] = -1;
    p.ranges[i * 2] = low;
    p.ranges[i * 2 + 1] = high;
    j :: Int64 := 0;
    while j < width {
        p.values[i * 64 + j] = def;
        p.defaults[i * 64 + j] = def;
        j = j + 1;
    }
    p.state[0] = i + 1;
    ss_document_source_change(d);
    d.state[8] = d.state[8] + 1;
    return true;
}

# Validate and update one current parameter component; return false for an unchanged value and avoid
# source recompilation.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     True when the current component changed; false for an unchanged value.
#
# Errors:
#     Unknown shader parameter/component.
fn ss_parameter_set(d :: SSDocument, id :: Int64, component :: Int64, value :: Float64) -> Bool {
    p :: SSParameters := d.parameters;
    s :: Int64 := ss_parameter_slot(p, id);
    if s < 0 || component < 0 || component >= p.meta[s * 8 + 3] {
        throw "Unknown shader parameter/component";
    }
    ss_parameter_validate(d, id, component, value);
    if p.values[s * 64 + component] == value {
        return false;
    }
    p.values[s * 64 + component] = value;
    d.state[8] = d.state[8] + 1;
    d.state[0] = d.state[0] + 1;
    return true;
}

# Read one current parameter component selected by stable ID and component index.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     component (Int64): Zero-based component within the selected parameter row.
#
# Returns:
#     The computed Float64 value: p.values[s * 64 + component].
#
# Errors:
#     Unknown shader parameter/component.
fn ss_parameter_get(d :: SSDocument, id :: Int64, component :: Int64) -> Float64 {
    p :: SSParameters := d.parameters;
    s :: Int64 := ss_parameter_slot(p, id);
    if s < 0 || component < 0 || component >= p.meta[s * 8 + 3] {
        throw "Unknown shader parameter/component";
    }
    return p.values[s * 64 + component];
}

# Validate a component's ID, index, GL-type constraints, and inclusive schema range without modifying
# its value.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Unknown shader parameter/component.
#     Parameter outside schema range.
fn ss_parameter_validate(d :: SSDocument, id :: Int64, component :: Int64, value :: Float64) -> Bool {
    p :: SSParameters := d.parameters;
    s :: Int64 := ss_parameter_slot(p, id);
    if s < 0 || component < 0 || component >= p.meta[s * 8 + 3] {
        throw "Unknown shader parameter/component";
    }
    ss_component_validate(p.meta[s * 8 + 1], value);
    if !(value >= p.ranges[s * 2] && value <= p.ranges[s * 2 + 1]) {
        throw "Parameter outside schema range";
    }
    return true;
}

# Validate and update one schema default, marking the document dirty only when it changes.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     True when the default component changed; false for an unchanged value.
fn ss_parameter_default_set(d :: SSDocument, id :: Int64, component :: Int64, value :: Float64) -> Bool {
    ss_parameter_validate(d, id, component, value);
    s :: Int64 := ss_parameter_slot(d.parameters, id);
    if d.parameters.defaults[s * 64 + component] == value {
        return false;
    }
    d.parameters.defaults[s * 64 + component] = value;
    d.state[0] = d.state[0] + 1;
    return true;
}

# Stage and validate a parameter's name/defaults/range/group/units/fit metadata before publication;
# renaming also advances the build revision.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     id (Int64): Positive stable resource/parameter/pass/node ID, not a native GPU token.
#     name (String): Display/resource/uniform name or environment-variable key, according to this API.
#     defaults (Array<Float64>): One default value per typed parameter component; shape must match the
#         schema row.
#     low (Float64): Inclusive lower limit.
#     high (Float64): Inclusive upper limit.
#     group_name (String): UI group label stored with the parameter schema.
#     units (String): Parameter units label for display.
#     continuous (Bool): Whether the parameter is eligible for continuous numeric adjustment.
#     fit (Bool): Whether the continuous scalar parameter is eligible for fitting.
#
# Returns:
#     True when retained metadata/defaults changed; false for an identical draft.
#
# Errors:
#     Unknown parameter or invalid uniform name.
#     Invalid metadata default shape/fit flags.
#     Duplicate parameter uniform name.
#     Metadata range must contain every default and retained current value.
fn ss_parameter_edit_metadata(
    d :: SSDocument,
    id :: Int64,
    name :: String,
    defaults :: Array<Float64>,
    low :: Float64,
    high :: Float64,
    group_name :: String,
    units :: String,
    continuous :: Bool,
    fit :: Bool
) -> Bool {
    p :: SSParameters := d.parameters;
    s :: Int64 := ss_parameter_slot(p, id);
    if s < 0 || !ss_identifier(name) {
        throw "Unknown parameter or invalid uniform name";
    }
    gl :: Int64 := p.meta[s * 8 + 1];
    width :: Int64 := p.meta[s * 8 + 3];
    if defaults.size() != width || (continuous && !ss_continuous(gl)) || (fit && !continuous) {
        throw "Invalid metadata default shape/fit flags";
    }
    ss_range_validate(gl, low, high);
    i :: Int64 := 0;
    while i < p.state[0] {
        if i != s && se_text_read(p.names[i]) == name {
            throw "Duplicate parameter uniform name";
        }
        i = i + 1;
    }
    i = 0;
    while i < width {
        ss_component_validate(gl, defaults[i]);
        ss_component_validate(gl, p.values[s * 64 + i]);
        if defaults[i] < low
            || defaults[i] > high
            || p.values[s * 64 + i] < low
            || p.values[s * 64 + i] > high {
            throw "Metadata range must contain every default and retained current value";
        }
        i = i + 1;
    }
    renamed :: Bool := se_text_read(p.names[s]) != name;
    changed :: Bool := renamed
        || se_text_read(p.groups[s]) != group_name
        || se_text_read(p.units[s]) != units
        || p.ranges[s * 2] != low
        || p.ranges[s * 2 + 1] != high
        || p.meta[s * 8 + 4] != int64(continuous)
        || p.meta[s * 8 + 5] != int64(fit);
    i = 0;
    while i < width {
        if p.defaults[s * 64 + i] != defaults[i] {
            changed = true;
        }
        i = i + 1;
    }
    @bounded<2097152> region parameter_metadata_edit {
        # Text capacity/UTF8 checks and whole-schema validation precede publication.
        staged :: SSParameters := ss_parameters();
        ss_parameters_copy(staged, p);
        se_text_write(staged.names[s], name);
        se_text_write(staged.groups[s], group_name);
        se_text_write(staged.units[s], units);
        staged.ranges[s * 2] = low;
        staged.ranges[s * 2 + 1] = high;
        staged.meta[s * 8 + 4] = int64(continuous);
        staged.meta[s * 8 + 5] = int64(fit);
        i = 0;
        while i < width {
            staged.defaults[s * 64 + i] = defaults[i];
            i = i + 1;
        }
        if renamed {
            staged.meta[s * 8 + 6] = 0;
            staged.meta[s * 8 + 7] = -1;
        }
        ss_parameters_validate(staged);
        if changed {
            ss_parameters_copy(p, staged);
        }
    };
    if changed {
        d.state[0] = d.state[0] + 1;
        if renamed {
            d.state[11] = d.state[11] + 1;
        }
    }
    return changed;
}

# Validate schema row counts, type/component shapes, ranges, and every default/current component.
#
# Parameters:
#     p (SSParameters): Typed parameter-schema rows, component values/defaults, ranges, and retained
#         labels.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Invalid shader parameter count.
#     Invalid shader parameter component shape.
#     Shader parameter default/current outside range.
fn ss_parameters_validate(p :: SSParameters) -> Bool {
    if p.state[0] < 0 || p.state[0] > p.names.size() {
        throw "Invalid shader parameter count";
    }
    i :: Int64 := 0;
    while i < p.state[0] {
        gl :: Int64 := p.meta[i * 8 + 1];
        width :: Int64 := p.meta[i * 8 + 3];
        if width < 1
            || width > 64
            || p.meta[i * 8 + 2] < 1
            || width != ss_components(gl) * p.meta[i * 8 + 2] {
            throw "Invalid shader parameter component shape";
        }
        ss_range_validate(gl, p.ranges[i * 2], p.ranges[i * 2 + 1]);
        j :: Int64 := 0;
        while j < width {
            ss_component_validate(gl, p.defaults[i * 64 + j]);
            ss_component_validate(gl, p.values[i * 64 + j]);
            if p.defaults[i * 64 + j] < p.ranges[i * 2]
                || p.defaults[i * 64 + j] > p.ranges[i * 2 + 1]
                || p.values[i * 64 + j] < p.ranges[i * 2]
                || p.values[i * 64 + j] > p.ranges[i * 2 + 1] {
                throw "Shader parameter default/current outside range";
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return true;
}

# Copy schema rows and retained labels into existing capacity-checked storage, clearing unused
# destination rows.
#
# Parameters:
#     to (SSParameters): Existing destination owner; its preallocated capacity must fit the copied
#         payload.
#     from (SSParameters): Source owner whose live payload is copied.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Retained shader parameter capacity exceeded.
fn ss_parameters_copy(to :: SSParameters, from :: SSParameters) -> Bool {
    if from.state[0] < 0 || from.state[0] > from.names.size() || from.state[0] > to.names.size() {
        throw "Retained shader parameter capacity exceeded";
    }
    i :: Int64 := 0;
    while i < to.meta.size() {
        value :: Int64 := 0;
        if i < from.meta.size() {
            value = from.meta[i];
        }
        to.meta[i] = value;
        i = i + 1;
    }
    i = 0;
    while i < to.values.size() {
        value :: Float64 := 0.0;
        def :: Float64 := 0.0;
        if i < from.values.size() {
            value = from.values[i];
            def = from.defaults[i];
        }
        to.values[i] = value;
        to.defaults[i] = def;
        i = i + 1;
    }
    i = 0;
    while i < to.ranges.size() {
        value :: Float64 := 0.0;
        if i < from.ranges.size() {
            value = from.ranges[i];
        }
        to.ranges[i] = value;
        i = i + 1;
    }
    i = 0;
    while i < to.names.size() {
        if i < from.names.size() {
            se_text_copy(to.names[i], from.names[i]);
            se_text_copy(to.groups[i], from.groups[i]);
            se_text_copy(to.units[i], from.units[i]);
        } else {
            name :: Array<Int64> := to.names[i];
            group_name :: Array<Int64> := to.groups[i];
            unit :: Array<Int64> := to.units[i];
            name[0] = 0;
            group_name[0] = 0;
            unit[0] = 0;
        }
        i = i + 1;
    }
    to.state[0] = from.state[0];
    return true;
}

# Serialize fragment/vertex source, include roots, schema metadata, and exact parameter values as a
# versioned shader resource.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_document_text(d :: SSDocument) -> String {
    b :: Int64 := vcore.sb_create();
    vcore.sb_append(
        b,
        "VMGE_SHADER|1\nF|"
            +se_text_hex(d.source)
            +"\nV|"
            +se_text_hex(d.vertex)
            +"\nFP|"
            +se_text_hex(d.fragment_path)
            +"\nVP|"
            +se_text_hex(d.vertex_path)
            +"\n"
    );
    p :: SSParameters := d.parameters;
    i :: Int64 := 0;
    while i < p.state[0] {
        vcore.sb_append(
            b,
            "P|"
                +string(p.meta[i * 8])
                +"|"
                +se_text_hex(p.names[i])
                +"|"
                +string(p.meta[i * 8 + 1])
                +"|"
                +string(p.meta[i * 8 + 2])
                +"|"
                +ss_encode(p.ranges[i * 2])
                +"|"
                +ss_encode(p.ranges[i * 2 + 1])
                +"|"
                +se_text_hex(p.groups[i])
                +"|"
                +se_text_hex(p.units[i])
                +"|"
                +string(p.meta[i * 8 + 4])
                +"|"
                +string(p.meta[i * 8 + 5])
        );
        j :: Int64 := 0;
        while j < p.meta[i * 8 + 3] {
            vcore.sb_append(
                b,
                "|" + ss_encode(p.defaults[i * 64 + j]) + "|" + ss_encode(p.values[i * 64 + j])
            );
            j = j + 1;
        }
        vcore.sb_append(b, "\n");
        i = i + 1;
    }
    return vcore.sb_build(b);
}

# Parse and validate a versioned shader resource into the supplied document staging owner.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Shader resource exceeds budget.
#     Unsupported shader resource version.
#     Incomplete parameter record.
#     Parameter record width.
#     Invalid parameter flags.
#     Invalid shader text record.
#     Unknown shader record.
#     Duplicate shader text record.
#     Missing shader text record.
fn ss_document_parse(d :: SSDocument, text :: String) -> Bool {
    if text.size() > 600000 {
        throw "Shader resource exceeds budget";
    }
    lines :: Array := ss_split(text, "\n");
    if lines[0] != "VMGE_SHADER|1" {
        throw "Unsupported shader resource version";
    }
    d.parameters.state[0] = 0;
    seen :: Int64 := 0;
    row :: Int64 := 1;
    while row < lines.size() {
        line :: String := lines[row];
        if line != "" {
            f :: Array := ss_split(line, "|");
            if f[0] == "P" {
                if f.size() < 13 {
                    throw "Incomplete parameter record";
                }
                gl :: Int64 := ss_integer(f[3]);
                len :: Int64 := ss_integer(f[4]);
                width :: Int64 := ss_components(gl) * len;
                if width < 1 || width > 64 || f.size() != 11 + width * 2 {
                    throw "Parameter record width";
                }
                name :: Array<Int64> := ss_text(127);
                group_name :: Array<Int64> := ss_text(127);
                units :: Array<Int64> := ss_text(63);
                ss_unhex(name, f[2]);
                ss_unhex(group_name, f[7]);
                ss_unhex(units, f[8]);
                c :: Int64 := ss_integer(f[9]);
                fit :: Int64 := ss_integer(f[10]);
                if c < 0 || c > 1 || fit < 0 || fit > 1 {
                    throw "Invalid parameter flags";
                }
                ss_parameter_define(
                    d,
                    ss_integer(f[1]),
                    se_text_read(name),
                    gl,
                    len,
                    ss_number(f[11]),
                    ss_number(f[5]),
                    ss_number(f[6]),
                    se_text_read(group_name),
                    se_text_read(units),
                    c == 1,
                    fit == 1
                );
                s :: Int64 := d.parameters.state[0] - 1;
                j :: Int64 := 0;
                while j < width {
                    ss_parameter_default_set(d, ss_integer(f[1]), j, ss_number(f[11 + j * 2]));
                    ss_parameter_set(d, ss_integer(f[1]), j, ss_number(f[12 + j * 2]));
                    j = j + 1;
                }
            } else {
                if f.size() != 2 {
                    throw "Invalid shader text record";
                }
                bit :: Int64 := 0;
                if f[0] == "F" {
                    bit = 1;
                    ss_unhex(d.source, f[1]);
                } else {
                    if f[0] == "V" {
                        bit = 2;
                        ss_unhex(d.vertex, f[1]);
                    } else {
                        if f[0] == "FP" {
                            bit = 4;
                            ss_unhex(d.fragment_path, f[1]);
                        } else {
                            if f[0] == "VP" {
                                bit = 8;
                                ss_unhex(d.vertex_path, f[1]);
                            } else {
                                throw "Unknown shader record";
                            }
                        }
                    }
                }
                if (seen / bit) % 2 != 0 {
                    throw "Duplicate shader text record";
                }
                seen = seen + bit;
            }
        }
        row = row + 1;
    }
    if seen != 15 {
        throw "Missing shader text record";
    }
    return true;
}

# Read and parse shader text in bounded candidate storage, then copy it into retained document owners
# without destroying last-good program bindings.
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
#     Cannot read shader (with contextual detail).
#     Retained shader document capacity exceeded.
fn ss_document_load(d :: SSDocument, path :: String) -> Bool {
    @bounded<33554432> region shader_load {
        raw := vfs.read(path);
        if raw == null {
            throw "Cannot read shader: " + path;
        }
        candidate :: SSDocument := ss_document();
        ss_document_parse(candidate, raw);
        se_text_write(candidate.path, path);
        tmp :: Array<Int64> := ss_text(600000);
        se_text_write(tmp, raw);
        h :: Int64 := ss_hash(tmp);
        if candidate.source[0] >= d.source.size()
            || candidate.vertex[0] >= d.vertex.size()
            || candidate.path[0] >= d.path.size()
            || candidate.fragment_path[0] >= d.fragment_path.size()
            || candidate.vertex_path[0] >= d.vertex_path.size()
            || candidate.parameters.state[0] > d.parameters.names.size() {
            throw "Retained shader document capacity exceeded";
        }
        se_text_copy(d.source, candidate.source);
        se_text_copy(d.vertex, candidate.vertex);
        se_text_copy(d.path, candidate.path);
        se_text_copy(d.fragment_path, candidate.fragment_path);
        se_text_copy(d.vertex_path, candidate.vertex_path);
        ss_parameters_copy(d.parameters, candidate.parameters);
        ss_document_source_change(d);
        d.state[1] = d.state[0];
        d.state[8] = d.state[8] + 1;
        d.state[6] = 0;
        d.state[7] = h;
    };
    return true;
}

# Atomically save the shader resource, then publish its retained path, hash, and clean disk revision.
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
#     Cannot save shader (with contextual detail).
fn ss_document_save(d :: SSDocument, path :: String) -> Bool {
    @bounded<33554432> region shader_save {
        staged_path :: Array<Int64> := ss_text(2048);
        se_text_write(staged_path, path);
        text :: String := ss_document_text(d);
        tmp :: Array<Int64> := ss_text(600000);
        se_text_write(tmp, text);
        h :: Int64 := ss_hash(tmp);
        if !vfs.write_atomic(path, text) {
            throw "Cannot save shader: " + path;
        }
        se_text_copy(d.path, staged_path);
        d.state[7] = h;
        d.state[1] = d.state[0];
        d.state[6] = 0;
    };
    return true;
}

# Poll the saved file hash and reload changed content only when local edits may be discarded;
# otherwise record an external-edit conflict.
#
# Parameters:
#     d (SSDocument): Retained shader source/schema/revision owner with a separate last-good published
#         program.
#     discard_local (Bool): Whether an explicit reload may replace unsaved local edits.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
#
# Errors:
#     Cannot read shader for reload.
fn ss_document_reload(d :: SSDocument, discard_local :: Bool) -> Bool {
    changed :: Bool := false;
    @bounded<16777216> region shader_poll {
        raw := vfs.read(se_text_read(d.path));
        if raw == null {
            throw "Cannot read shader for reload";
        }
        tmp :: Array<Int64> := ss_text(600000);
        se_text_write(tmp, raw);
        changed = ss_hash(tmp) != d.state[7];
    };
    if !changed {
        return false;
    }
    if ss_document_dirty(d) && !discard_local {
        d.state[6] = 1;
        se_text_write(d.log, "External shader change conflicts with local edits; explicitly reload or save.");
        return false;
    }
    return ss_document_load(d, se_text_read(d.path));
}
