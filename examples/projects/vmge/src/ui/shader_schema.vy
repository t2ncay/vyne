use "../shader/material.vy";
use native vmem;

# Small preframe owners: reflection cache (64 rows), one metadata draft and eight
# selective value transactions. No document/material clone is retained here.
interface SSShaderPanel {
    reflection :: Array<Int64>, names :: Array<Int64>, state :: Array<Int64>,
    draft :: Array<Int64>, numbers :: Array<Float64>, name :: Array<Int64>,
    group_name :: Array<Int64>, units :: Array<Int64>,
    history :: Array<Int64>, history_values :: Array<Float64>, before :: Array<Float64>, history_state :: Array<Int64>,
}
fn ss_shader_panel() -> SSShaderPanel {
    before :: Int64 := vmem.total_allocated();
    panel :: SSShaderPanel := SSShaderPanel(ss_ints(256),ss_ints(8192),ss_ints(32),ss_ints(16),ss_floats(130),ss_text(127),ss_text(127),ss_text(63),ss_ints(32),ss_floats(2048),ss_floats(128),ss_ints(16));
    panel.state[20] = vmem.total_allocated()-before; return panel;
}
# reflection stride 4: GLtype, cardinality, location, support code.
fn ss_uniform_builtin(name :: String) -> Bool {
    if name == "mvp" || name == "matModel" || name == "matNormal" || name == "matView" || name == "matProjection" || name == "view" || name == "projection" || name == "colDiffuse" || name == "colSpecular" || name == "vertexPosition" { return true; }
    i :: Int64 := 0; while i < 12 { if name == "texture"+string(i) { return true; } i = i+1; }
    if name.size() < 3 { return false; } return name[0] == "g" && name[1] == "l" && name[2] == "_";
}
fn ss_uniform_base(name :: String) -> String {
    n :: Int64 := name.size(); if n >= 3 { if name[n-3] == "[" && name[n-2] == "0" && name[n-1] == "]" { return vraylib.text_subtext(name,0,n-3); } } return name;
}
fn ss_uniform_support(name :: String,gl :: Int64,count :: Int64,location :: Int64) -> Int64 {
    if location < 0 { return 1; } if !ss_identifier(name) { return 2; }
    if !ss_upload_supported(gl) { return 3; } if gl == 35678 && count != 1 { return 4; }
    if count < 1 || ss_components(gl)*count > 64 { return 5; } return 0;
}
fn ss_uniform_support_text(code :: Int64) -> String {
    if code == 1 { return "Unsupported: block / negative location"; }
    if code == 2 { return "Unsupported: block member or name"; }
    if code == 3 { return "Unsupported: GL upload type"; }
    if code == 4 { return "Unsupported: sampler array"; }
    if code == 5 { return "Unsupported: component/name capacity"; } return "Ready to expose";
}
fn ss_panel_name(panel :: SSShaderPanel,row :: Int64) -> String {
    return se_edit_slice(panel.names,row*128,row*128+panel.names[row*128]);
}
fn ss_panel_reflect(panel :: SSShaderPanel,d :: SSDocument,rows :: Array) -> Bool {
    panel.state[0] = 0; panel.state[1] = 0; i :: Int64 := 0;
    while i < rows.size() {
        row :: Array := rows[i]; name :: String := ss_uniform_base(row[0]); represented :: Bool := ss_uniform_builtin(name); j :: Int64 := 0;
        while j < d.parameters.state[0] { if se_text_read(d.parameters.names[j]) == name { represented = true; } j = j+1; }
        if !represented {
            n :: Int64 := panel.state[0]; if n == 64 { panel.state[1] = 1; } else {
                length :: Int64 := int64(vmath.min(127.0,float64(name.size()))); panel.names[n*128] = length; j = 0;
                while j < length { panel.names[n*128+j+1] = name.byte_at(j); j = j+1; }
                panel.reflection[n*4] = int64(row[1]); panel.reflection[n*4+1] = int64(row[2]); panel.reflection[n*4+2] = int64(row[3]);
                code :: Int64 := ss_uniform_support(name,int64(row[1]),int64(row[2]),int64(row[3])); if name.size() > 127 { code = 5; }
                panel.reflection[n*4+3] = code; panel.state[0] = n+1;
            }
        } i = i+1;
    } return true;
}
# draft: active, stable ID, GLtype, arraylen, width, continuous, fit, existing,
# baseline edit revision, reflected location. numbers: low/high, defaults[64], current[64].
fn ss_schema_begin(panel :: SSShaderPanel,d :: SSDocument,id :: Int64,name :: String,gl :: Int64,count :: Int64,location :: Int64,existing :: Bool) -> Bool {
    if ss_uniform_support(name,gl,count,location) != 0 { throw "Uniform cannot be exposed by this runtime"; }
    se_text_write(panel.name,name); panel.draft[0] = 1; panel.draft[1] = id; panel.draft[2] = gl; panel.draft[3] = count; panel.draft[4] = ss_components(gl)*count;
    panel.draft[5] = int64(ss_continuous(gl)); panel.draft[6] = 0; panel.draft[7] = int64(existing); panel.draft[8] = d.state[0]; panel.draft[9] = location;
    panel.numbers[0] = 0.0; panel.numbers[1] = 1.0; def :: Float64 := 1.0;
    if !ss_continuous(gl) { def = 0.0; } if gl == 5124 || (gl >= 35667 && gl <= 35669) || gl == 35676 { panel.numbers[0] = -16.0; panel.numbers[1] = 16.0; }
    se_text_write(panel.group_name,"Surface"); panel.units[0] = 0; slot :: Int64 := ss_parameter_slot(d.parameters,id);
    if existing && slot < 0 { throw "Parameter no longer exists"; }
    same_type :: Bool := false; if existing { same_type = d.parameters.meta[slot*8+1] == gl; se_text_copy(panel.group_name,d.parameters.groups[slot]); se_text_copy(panel.units,d.parameters.units[slot]); if same_type { panel.numbers[0] = d.parameters.ranges[slot*2]; panel.numbers[1] = d.parameters.ranges[slot*2+1]; panel.draft[5] = d.parameters.meta[slot*8+4]; panel.draft[6] = d.parameters.meta[slot*8+5]; } }
    i :: Int64 := 0; while i < panel.draft[4] {
        value :: Float64 := def; if gl == 35676 { value = 0.0; if (i%16)%5 == 0 { value = 1.0; } }
        panel.numbers[i+2] = value; panel.numbers[i+66] = value;
        if same_type { if i < d.parameters.meta[slot*8+3] { panel.numbers[i+2] = d.parameters.defaults[slot*64+i]; panel.numbers[i+66] = d.parameters.values[slot*64+i]; } } i = i+1;
    } return true;
}
fn ss_schema_validate(panel :: SSShaderPanel,d :: SSDocument) -> Bool {
    if panel.draft[0] == 0 || panel.draft[8] != d.state[0] { throw "Document changed; reopen the schema draft"; }
    gl :: Int64 := panel.draft[2]; width :: Int64 := panel.draft[4];
    if panel.draft[1] < 1 || panel.draft[1] > 1000000 || ss_uniform_support(se_text_read(panel.name),gl,panel.draft[3],panel.draft[9]) != 0 || width != ss_components(gl)*panel.draft[3] { throw "Invalid reflected parameter shape"; }
    if panel.draft[5] < 0 || panel.draft[5] > 1 || panel.draft[6] < 0 || panel.draft[6] > 1 || (panel.draft[5] == 1 && !ss_continuous(gl)) || (panel.draft[6] == 1 && panel.draft[5] == 0) { throw "Continuous / fit flags do not match the type"; }
    ss_range_validate(gl,panel.numbers[0],panel.numbers[1]); ss_utf8_validate(panel.group_name); ss_utf8_validate(panel.units);
    i :: Int64 := 0; while i < width { ss_component_validate(gl,panel.numbers[i+2]); ss_component_validate(gl,panel.numbers[i+66]); if panel.numbers[i+2] < panel.numbers[0] || panel.numbers[i+2] > panel.numbers[1] || panel.numbers[i+66] < panel.numbers[0] || panel.numbers[i+66] > panel.numbers[1] { throw "Every default/current component must fit the new range"; } i = i+1; } return true;
}
fn ss_schema_commit(panel :: SSShaderPanel,d :: SSDocument,m :: MaterialInstance) -> Bool {
    ss_schema_validate(panel,d);
    @bounded<16777216> region shader_ui_schema_commit {
        candidate :: SSDocument := ss_document(); ss_parameters_copy(candidate.parameters,d.parameters); p :: SSParameters := candidate.parameters; id :: Int64 := panel.draft[1]; slot :: Int64 := ss_parameter_slot(p,id);
        if panel.draft[7] == 0 { if slot >= 0 { throw "Stable parameter ID is already used"; } ss_parameter_define(candidate,id,se_text_read(panel.name),panel.draft[2],panel.draft[3],panel.numbers[2],panel.numbers[0],panel.numbers[1],se_text_read(panel.group_name),se_text_read(panel.units),panel.draft[5] == 1,panel.draft[6] == 1); slot = ss_parameter_slot(p,id); }
        else { if slot < 0 || se_text_read(p.names[slot]) != se_text_read(panel.name) { throw "Schema target changed"; } }
        p.meta[slot*8+1] = panel.draft[2]; p.meta[slot*8+2] = panel.draft[3]; p.meta[slot*8+3] = panel.draft[4]; p.meta[slot*8+4] = panel.draft[5]; p.meta[slot*8+5] = panel.draft[6]; p.meta[slot*8+6] = 0; p.meta[slot*8+7] = -1;
        p.ranges[slot*2] = panel.numbers[0]; p.ranges[slot*2+1] = panel.numbers[1]; se_text_copy(p.groups[slot],panel.group_name); se_text_copy(p.units[slot],panel.units);
        i :: Int64 := 0; while i < panel.draft[4] { p.defaults[slot*64+i] = panel.numbers[i+2]; p.values[slot*64+i] = panel.numbers[i+66]; i = i+1; }
        ss_parameters_validate(p); material :: MaterialInstance := ss_material_like(m); ss_material_copy(material,m); ss_material_migrate(material,candidate);
        # Both candidates validated before either retained owner changes.
        ss_parameters_copy(d.parameters,p); ss_material_copy(m,material); ss_document_source_change(d); d.state[8] = d.state[8]+1;
    }; panel.draft[0] = 0; return true;
}
fn ss_value_history_clear(panel :: SSShaderPanel) -> Bool { i :: Int64 := 0; while i < panel.history_state.size() { panel.history_state[i] = 0; i = i+1; } return true; }
fn ss_value_begin(panel :: SSShaderPanel,d :: SSDocument,m :: MaterialInstance,id :: Int64) -> Bool {
    if panel.history_state[0] != 0 { if panel.history_state[1] == id { return true; } ss_value_end(panel,d,m); }
    p :: SSParameters := d.parameters; slot :: Int64 := ss_parameter_slot(p,id); if slot < 0 { throw "Unknown parameter transaction"; }
    panel.history_state[0] = 1; panel.history_state[1] = id; panel.history_state[2] = p.meta[slot*8+1]; panel.history_state[3] = p.meta[slot*8+2]; panel.history_state[4] = p.meta[slot*8+3];
    other :: Int64 := ss_material_compatible_slot(m,id,p.meta[slot*8+1],p.meta[slot*8+2]); i :: Int64 := 0;
    while i < panel.history_state[4] { panel.before[i] = p.values[slot*64+i]; panel.before[i+64] = panel.before[i]; if other >= 0 { panel.before[i+64] = m.values[other*64+i]; } i = i+1; } return true;
}
fn ss_value_end(panel :: SSShaderPanel,d :: SSDocument,m :: MaterialInstance) -> Bool {
    if panel.history_state[0] == 0 { return false; } panel.history_state[0] = 0;
    id :: Int64 := panel.history_state[1]; slot :: Int64 := ss_parameter_slot(d.parameters,id); other :: Int64 := ss_material_compatible_slot(m,id,panel.history_state[2],panel.history_state[3]);
    if slot < 0 || other < 0 || d.parameters.meta[slot*8+1] != panel.history_state[2] || d.parameters.meta[slot*8+2] != panel.history_state[3] { return false; }
    changed :: Bool := false; i :: Int64 := 0; while i < panel.history_state[4] { if panel.before[i] != d.parameters.values[slot*64+i] || panel.before[i+64] != m.values[other*64+i] { changed = true; } i = i+1; } if !changed { return false; }
    cursor :: Int64 := panel.history_state[5]; if cursor == 8 { panel.history_state[7] = (panel.history_state[7]+1)%8; cursor = 7; }
    row :: Int64 := (panel.history_state[7]+cursor)%8; i = 0; while i < 4 { panel.history[row*4+i] = panel.history_state[i+1]; i = i+1; }
    i = 0; while i < panel.history_state[4] { panel.history_values[row*256+i] = panel.before[i]; panel.history_values[row*256+i+64] = panel.before[i+64]; panel.history_values[row*256+i+128] = d.parameters.values[slot*64+i]; panel.history_values[row*256+i+192] = m.values[other*64+i]; i = i+1; }
    panel.history_state[5] = cursor+1; panel.history_state[6] = cursor+1; return true;
}
fn ss_value_undo(panel :: SSShaderPanel,d :: SSDocument,m :: MaterialInstance,redo :: Bool) -> Bool {
    if panel.history_state[0] != 0 { ss_value_end(panel,d,m); }
    logical :: Int64 := panel.history_state[5]-1; if redo { logical = panel.history_state[5]; } if logical < 0 || logical >= panel.history_state[6] { return false; }
    row :: Int64 := (panel.history_state[7]+logical)%8; id :: Int64 := panel.history[row*4]; slot :: Int64 := ss_parameter_slot(d.parameters,id);
    if slot < 0 || d.parameters.meta[slot*8+1] != panel.history[row*4+1] || d.parameters.meta[slot*8+2] != panel.history[row*4+2] { throw "Undo parameter shape changed"; }
    offset :: Int64 := 0; if redo { offset = 128; }
    @bounded<16777216> region shader_ui_value_undo {
        candidate :: SSDocument := ss_document(); ss_parameters_copy(candidate.parameters,d.parameters); material :: MaterialInstance := ss_material_like(m); ss_material_copy(material,m); ss_material_migrate(material,candidate);
        i :: Int64 := 0; while i < panel.history[row*4+3] { value :: Float64 := panel.history_values[row*256+offset+i]; override :: Float64 := panel.history_values[row*256+offset+i+64]; ss_parameter_validate(candidate,id,i,override); ss_parameter_set(candidate,id,i,value); ss_material_set(material,id,i,override); i = i+1; }
        ss_parameters_validate(candidate.parameters); ss_material_validate(material); ss_parameters_copy(d.parameters,candidate.parameters); ss_material_copy(m,material); d.state[0] = d.state[0]+1; d.state[8] = d.state[8]+1;
    }; if redo { panel.history_state[5] = logical+1; } else { panel.history_state[5] = logical; } return true;
}
