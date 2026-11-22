use "common.vy";

# Parameter meta stride 8: id, GLtype, arraylen, width, continuous, fit, reflection status, location.
# Status: 0 uncompiled, 1 active, 2 missing, 3 incompatible. Values/default stride 64.
interface SSParameters { meta :: Array<Int64>, values :: Array<Float64>, defaults :: Array<Float64>, ranges :: Array<Float64>, names :: Array, groups :: Array, units :: Array, state :: Array<Int64> }
interface SSDocument {
    source :: Array<Int64>, vertex :: Array<Int64>, path :: Array<Int64>, fragment_path :: Array<Int64>, vertex_path :: Array<Int64>, state :: Array<Int64>, parameters :: SSParameters, log :: Array<Int64>, dependencies :: Array, dependency_hashes :: Array<Int64>, published :: SSParameters, retired :: Array<Int64>, retired_sessions :: Array<Int64>
}
fn ss_parameters() -> SSParameters {
    names :: Array := []; groups :: Array := []; units :: Array := []; i :: Int64 := 0;
    while i < 64 { names.push(ss_text(127)); groups.push(ss_text(127)); units.push(ss_text(63)); i = i+1; }
    return SSParameters(ss_ints(512),ss_floats(4096),ss_floats(4096),ss_floats(128),names,groups,units,ss_ints(1));
}
fn ss_document() -> SSDocument {
    deps :: Array := []; i :: Int64 := 0; while i < 32 { deps.push(ss_text(2048)); i = i+1; }
    d :: SSDocument := SSDocument(ss_text(65536),ss_text(65536),ss_text(2048),ss_text(2048),ss_text(2048),ss_ints(16),ss_parameters(),ss_text(16384),deps,ss_ints(32),ss_parameters(),ss_ints(16),ss_ints(16)); d.state[0] = 1; d.state[11] = 1;
    se_text_write(d.source,"#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nuniform float gain;\nvoid main()\{finalColor=vec4(vec3(gain),1.0);}\n");
    ss_parameter_define(d,1,"gain",5126,1,0.5,0.0,1.0,"Surface","linear",true,true); return d;
}
fn ss_document_source_change(d :: SSDocument) -> Bool { d.state[0] = d.state[0]+1; d.state[11] = d.state[11]+1; return true; }
fn ss_document_dirty(d :: SSDocument) -> Bool { return d.state[0] != d.state[1]; }
fn ss_document_compile_dirty(d :: SSDocument) -> Bool { return d.state[11] != d.state[2]; }
fn ss_document_applied(d :: SSDocument) -> Bool { return !ss_document_compile_dirty(d) && d.state[3] == d.state[2] && d.state[2] != 0 && d.state[9] == d.state[8]; }
fn ss_document_log(d :: SSDocument,text :: String) -> Bool {
    if text.size() <= 3500 { return se_text_write(d.log,text); } b :: Int64 := vcore.sb_create(); i :: Int64 := 0; while i < 3500 { vcore.sb_append(b,text[i]); i = i+1; } vcore.sb_append(b,"\n[diagnostic truncated]"); return se_text_write(d.log,vcore.sb_build(b));
}
fn ss_parameter_slot(p :: SSParameters,id :: Int64) -> Int64 { i :: Int64 := 0; while i < p.state[0] { if p.meta[i*8] == id { return i; } i = i+1; } return -1; }
fn ss_parameter_define(d :: SSDocument,id :: Int64,name :: String,gltype :: Int64,arraylen :: Int64,def :: Float64,low :: Float64,high :: Float64,group_name :: String,units :: String,continuous :: Bool,fit :: Bool) -> Bool {
    p :: SSParameters := d.parameters; width :: Int64 := ss_components(gltype)*arraylen;
    if id < 1 || id > 1000000 || !ss_identifier(name) || arraylen < 1 || width < 1 || width > 64 || !(low <= def && def <= high) || (continuous && !ss_continuous(gltype)) || (fit && !continuous) { throw "Invalid shader parameter schema"; }
    ss_range_validate(gltype,low,high); ss_component_validate(gltype,def);
    i :: Int64 := 0; while i < p.state[0] { if p.meta[i*8] == id || se_text_read(p.names[i]) == name { throw "Duplicate shader parameter ID/name"; } i = i+1; }
    if i == 64 || group_name.size() > 127 || units.size() > 63 { throw "Shader parameter capacity"; }
    se_text_write(p.names[i],name); se_text_write(p.groups[i],group_name); se_text_write(p.units[i],units);
    p.meta[i*8] = id; p.meta[i*8+1] = gltype; p.meta[i*8+2] = arraylen; p.meta[i*8+3] = width; p.meta[i*8+4] = int64(continuous); p.meta[i*8+5] = int64(fit); p.meta[i*8+6] = 0; p.meta[i*8+7] = -1;
    p.ranges[i*2] = low; p.ranges[i*2+1] = high; j :: Int64 := 0; while j < width { p.values[i*64+j] = def; p.defaults[i*64+j] = def; j = j+1; }
    p.state[0] = i+1; ss_document_source_change(d); d.state[8] = d.state[8]+1; return true;
}
fn ss_parameter_set(d :: SSDocument,id :: Int64,component :: Int64,value :: Float64) -> Bool {
    p :: SSParameters := d.parameters; s :: Int64 := ss_parameter_slot(p,id);
    if s < 0 || component < 0 || component >= p.meta[s*8+3] { throw "Unknown shader parameter/component"; }
    ss_parameter_validate(d,id,component,value);
    if p.values[s*64+component] == value { return false; } p.values[s*64+component] = value; d.state[8] = d.state[8]+1; d.state[0] = d.state[0]+1; return true;
}
fn ss_parameter_get(d :: SSDocument,id :: Int64,component :: Int64) -> Float64 { p :: SSParameters := d.parameters; s :: Int64 := ss_parameter_slot(p,id); if s < 0 || component < 0 || component >= p.meta[s*8+3] { throw "Unknown shader parameter/component"; } return p.values[s*64+component]; }
fn ss_parameter_validate(d :: SSDocument,id :: Int64,component :: Int64,value :: Float64) -> Bool {
    p :: SSParameters := d.parameters; s :: Int64 := ss_parameter_slot(p,id); if s < 0 || component < 0 || component >= p.meta[s*8+3] { throw "Unknown shader parameter/component"; }
    ss_component_validate(p.meta[s*8+1],value); if !(value >= p.ranges[s*2] && value <= p.ranges[s*2+1]) { throw "Parameter outside schema range"; } return true;
}
fn ss_parameter_default_set(d :: SSDocument,id :: Int64,component :: Int64,value :: Float64) -> Bool {
    ss_parameter_validate(d,id,component,value); s :: Int64 := ss_parameter_slot(d.parameters,id); if d.parameters.defaults[s*64+component] == value { return false; } d.parameters.defaults[s*64+component] = value; d.state[0] = d.state[0]+1; return true;
}
fn ss_parameter_edit_metadata(d :: SSDocument,id :: Int64,name :: String,defaults :: Array<Float64>,low :: Float64,high :: Float64,group_name :: String,units :: String,continuous :: Bool,fit :: Bool) -> Bool {
    p :: SSParameters := d.parameters; s :: Int64 := ss_parameter_slot(p,id); if s < 0 || !ss_identifier(name) { throw "Unknown parameter or invalid uniform name"; }
    gl :: Int64 := p.meta[s*8+1]; width :: Int64 := p.meta[s*8+3]; if defaults.size() != width || (continuous && !ss_continuous(gl)) || (fit && !continuous) { throw "Invalid metadata default shape/fit flags"; }
    ss_range_validate(gl,low,high); i :: Int64 := 0; while i < p.state[0] { if i != s && se_text_read(p.names[i]) == name { throw "Duplicate parameter uniform name"; } i = i+1; }
    i = 0; while i < width { ss_component_validate(gl,defaults[i]); ss_component_validate(gl,p.values[s*64+i]); if defaults[i] < low || defaults[i] > high || p.values[s*64+i] < low || p.values[s*64+i] > high { throw "Metadata range must contain every default and retained current value"; } i = i+1; }
    renamed :: Bool := se_text_read(p.names[s]) != name; changed :: Bool := renamed || se_text_read(p.groups[s]) != group_name || se_text_read(p.units[s]) != units || p.ranges[s*2] != low || p.ranges[s*2+1] != high || p.meta[s*8+4] != int64(continuous) || p.meta[s*8+5] != int64(fit);
    i = 0; while i < width { if p.defaults[s*64+i] != defaults[i] { changed = true; } i = i+1; }
    @bounded<2097152> region parameter_metadata_edit {
        # Text capacity/UTF8 checks and whole-schema validation precede publication.
        staged :: SSParameters := ss_parameters(); ss_parameters_copy(staged,p); se_text_write(staged.names[s],name); se_text_write(staged.groups[s],group_name); se_text_write(staged.units[s],units);
        staged.ranges[s*2] = low; staged.ranges[s*2+1] = high; staged.meta[s*8+4] = int64(continuous); staged.meta[s*8+5] = int64(fit); i = 0; while i < width { staged.defaults[s*64+i] = defaults[i]; i = i+1; }
        if renamed { staged.meta[s*8+6] = 0; staged.meta[s*8+7] = -1; } ss_parameters_validate(staged); if changed { ss_parameters_copy(p,staged); }
    };
    if changed { d.state[0] = d.state[0]+1; if renamed { d.state[11] = d.state[11]+1; } } return changed;
}
fn ss_parameters_validate(p :: SSParameters) -> Bool {
    if p.state[0] < 0 || p.state[0] > 64 { throw "Invalid shader parameter count"; } i :: Int64 := 0;
    while i < p.state[0] { gl :: Int64 := p.meta[i*8+1]; width :: Int64 := p.meta[i*8+3]; if width < 1 || width > 64 || p.meta[i*8+2] < 1 || width != ss_components(gl)*p.meta[i*8+2] { throw "Invalid shader parameter component shape"; } ss_range_validate(gl,p.ranges[i*2],p.ranges[i*2+1]); j :: Int64 := 0;
        while j < width { ss_component_validate(gl,p.defaults[i*64+j]); ss_component_validate(gl,p.values[i*64+j]); if p.defaults[i*64+j] < p.ranges[i*2] || p.defaults[i*64+j] > p.ranges[i*2+1] || p.values[i*64+j] < p.ranges[i*2] || p.values[i*64+j] > p.ranges[i*2+1] { throw "Shader parameter default/current outside range"; } j = j+1; } i = i+1;
    } return true;
}
fn ss_parameters_copy(to :: SSParameters,from :: SSParameters) -> Bool {
    i :: Int64 := 0; while i < 512 { to.meta[i] = from.meta[i]; i = i+1; } i = 0; while i < 4096 { to.values[i] = from.values[i]; to.defaults[i] = from.defaults[i]; i = i+1; } i = 0;
    while i < 64 { to.ranges[i*2] = from.ranges[i*2]; to.ranges[i*2+1] = from.ranges[i*2+1]; se_text_copy(to.names[i],from.names[i]); se_text_copy(to.groups[i],from.groups[i]); se_text_copy(to.units[i],from.units[i]); i = i+1; } to.state[0] = from.state[0]; return true;
}
fn ss_document_text(d :: SSDocument) -> String {
    b :: Int64 := vcore.sb_create(); vcore.sb_append(b,"VMGE_SHADER|1\nF|"+se_text_hex(d.source)+"\nV|"+se_text_hex(d.vertex)+"\nFP|"+se_text_hex(d.fragment_path)+"\nVP|"+se_text_hex(d.vertex_path)+"\n");
    p :: SSParameters := d.parameters; i :: Int64 := 0; while i < p.state[0] {
        vcore.sb_append(b,"P|"+string(p.meta[i*8])+"|"+se_text_hex(p.names[i])+"|"+string(p.meta[i*8+1])+"|"+string(p.meta[i*8+2])+"|"+ss_encode(p.ranges[i*2])+"|"+ss_encode(p.ranges[i*2+1])+"|"+se_text_hex(p.groups[i])+"|"+se_text_hex(p.units[i])+"|"+string(p.meta[i*8+4])+"|"+string(p.meta[i*8+5]));
        j :: Int64 := 0; while j < p.meta[i*8+3] { vcore.sb_append(b,"|"+ss_encode(p.defaults[i*64+j])+"|"+ss_encode(p.values[i*64+j])); j = j+1; } vcore.sb_append(b,"\n"); i = i+1;
    } return vcore.sb_build(b);
}
fn ss_document_parse(d :: SSDocument,text :: String) -> Bool {
    if text.size() > 600000 { throw "Shader resource exceeds budget"; } lines :: Array := ss_split(text,"\n"); if lines[0] != "VMGE_SHADER|1" { throw "Unsupported shader resource version"; }
    d.parameters.state[0] = 0; seen :: Int64 := 0; row :: Int64 := 1;
    while row < lines.size() { line :: String := lines[row]; if line != "" {
        f :: Array := ss_split(line,"|"); if f[0] == "P" {
            if f.size() < 13 { throw "Incomplete parameter record"; } gl :: Int64 := ss_integer(f[3]); len :: Int64 := ss_integer(f[4]); width :: Int64 := ss_components(gl)*len; if width < 1 || width > 64 || f.size() != 11+width*2 { throw "Parameter record width"; }
            name :: Array<Int64> := ss_text(127); group_name :: Array<Int64> := ss_text(127); units :: Array<Int64> := ss_text(63); ss_unhex(name,f[2]); ss_unhex(group_name,f[7]); ss_unhex(units,f[8]); c :: Int64 := ss_integer(f[9]); fit :: Int64 := ss_integer(f[10]); if c < 0 || c > 1 || fit < 0 || fit > 1 { throw "Invalid parameter flags"; }
            ss_parameter_define(d,ss_integer(f[1]),se_text_read(name),gl,len,ss_number(f[11]),ss_number(f[5]),ss_number(f[6]),se_text_read(group_name),se_text_read(units),c == 1,fit == 1);
            s :: Int64 := d.parameters.state[0]-1; j :: Int64 := 0; while j < width { ss_parameter_default_set(d,ss_integer(f[1]),j,ss_number(f[11+j*2])); ss_parameter_set(d,ss_integer(f[1]),j,ss_number(f[12+j*2])); j = j+1; }
        } else {
            if f.size() != 2 { throw "Invalid shader text record"; } bit :: Int64 := 0;
            if f[0] == "F" { bit = 1; ss_unhex(d.source,f[1]); } else { if f[0] == "V" { bit = 2; ss_unhex(d.vertex,f[1]); } else { if f[0] == "FP" { bit = 4; ss_unhex(d.fragment_path,f[1]); } else { if f[0] == "VP" { bit = 8; ss_unhex(d.vertex_path,f[1]); } else { throw "Unknown shader record"; } } } }
            if (seen/bit)%2 != 0 { throw "Duplicate shader text record"; } seen = seen+bit;
        }
    } row = row+1; } if seen != 15 { throw "Missing shader text record"; } return true;
}
fn ss_document_load(d :: SSDocument,path :: String) -> Bool {
    @bounded<33554432> region shader_load {
        raw := vfs.read(path); if raw == null { throw "Cannot read shader: "+path; } candidate :: SSDocument := ss_document(); ss_document_parse(candidate,raw); se_text_write(candidate.path,path); tmp :: Array<Int64> := ss_text(600000); se_text_write(tmp,raw); h :: Int64 := ss_hash(tmp);
        se_text_copy(d.source,candidate.source); se_text_copy(d.vertex,candidate.vertex); se_text_copy(d.path,candidate.path); se_text_copy(d.fragment_path,candidate.fragment_path); se_text_copy(d.vertex_path,candidate.vertex_path); ss_parameters_copy(d.parameters,candidate.parameters);
        ss_document_source_change(d); d.state[1] = d.state[0]; d.state[8] = d.state[8]+1; d.state[6] = 0; d.state[7] = h;
    }; return true;
}
fn ss_document_save(d :: SSDocument,path :: String) -> Bool {
    @bounded<33554432> region shader_save { staged_path :: Array<Int64> := ss_text(2048); se_text_write(staged_path,path); text :: String := ss_document_text(d); tmp :: Array<Int64> := ss_text(600000); se_text_write(tmp,text); h :: Int64 := ss_hash(tmp); if !vfs.write_atomic(path,text) { throw "Cannot save shader: "+path; } se_text_copy(d.path,staged_path); d.state[7] = h; d.state[1] = d.state[0]; d.state[6] = 0; }; return true;
}
fn ss_document_reload(d :: SSDocument,discard_local :: Bool) -> Bool {
    changed :: Bool := false;
    @bounded<16777216> region shader_poll { raw := vfs.read(se_text_read(d.path)); if raw == null { throw "Cannot read shader for reload"; } tmp :: Array<Int64> := ss_text(600000); se_text_write(tmp,raw); changed = ss_hash(tmp) != d.state[7]; };
    if !changed { return false; } if ss_document_dirty(d) && !discard_local { d.state[6] = 1; se_text_write(d.log,"External shader change conflicts with local edits; explicitly reload or save."); return false; } return ss_document_load(d,se_text_read(d.path));
}
