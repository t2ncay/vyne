use "document.vy";

# Types: 1 float, 2 vec2, 3 vec3, 4 vec4, 5 sampler2D.
# Kinds: 0 constant, 1 exposed parameter, 2 add, 3 multiply, 4 mix,
# 5 UV, 6 texture sample, 7 fragment output, 8 subtract, 9 divide.
# Meta stride 8: ID, kind, type, input A/B/C IDs (-1 disconnected), parameter ID, reserved.
interface SSGraph { meta :: Array<Int64>, values :: Array<Float64>, names :: Array, state :: Array<Int64>, work :: Array<Int64>, source :: Array<Int64>, log :: Array<Int64> }
fn ss_graph() -> SSGraph { names :: Array := []; i :: Int64 := 0; while i < 64 { names.push(ss_text(127)); i = i+1; } return SSGraph(ss_ints(512),ss_floats(384),names,ss_ints(3),ss_ints(128),ss_text(65536),ss_text(4096)); }
fn ss_graph_slot(g :: SSGraph,id :: Int64) -> Int64 { i :: Int64 := 0; while i < g.state[0] { if g.meta[i*8] == id { return i; } i = i+1; } return -1; }
fn ss_graph_exposed_name(name :: String) -> Bool {
    if !ss_identifier(name) { return false; }
    if name.size() >= 2 && name[0] == "n" && name[1] == "_" { return false; } if name.size() >= 3 && name[0] == "g" && name[1] == "l" && name[2] == "_" { return false; }
    previous :: String := ""; i :: Int64 := 0; while i < name.size() { if previous == "_" && name[i] == "_" { return false; } previous = name[i]; i = i+1; }
    words :: Array := ss_split("finalColor|fragTexCoord|fragColor|main|texture|mix|exp2|attribute|const|uniform|varying|layout|centroid|flat|smooth|noperspective|break|continue|do|for|while|switch|case|default|if|else|in|out|inout|float|double|int|uint|void|bool|true|false|invariant|discard|return|struct|precision|lowp|mediump|highp|mat2|mat3|mat4|vec2|vec3|vec4|ivec2|ivec3|ivec4|bvec2|bvec3|bvec4|uvec2|uvec3|uvec4|dvec2|dvec3|dvec4|sampler1D|sampler2D|sampler3D|samplerCube|sampler2DShadow|sampler2DArray|isampler2D|usampler2D|mat2x2|mat2x3|mat2x4|mat3x2|mat3x3|mat3x4|mat4x2|mat4x3|mat4x4|dmat2|dmat3|dmat4|common|partition|active|asm|class|union|enum|typedef|template|this|packed|goto|inline|noinline|volatile|public|static|extern|external|interface|long|short|half|fixed|unsigned|superp|input|output|hvec2|hvec3|hvec4|fvec2|fvec3|fvec4|sampler3DRect|filter|sizeof|cast|namespace|using","|");
    i = 0; while i < words.size() { if name == words[i] { return false; } i = i+1; }
    extra :: Array := ss_split("mvp|colDiffuse|sampler2DRect|sampler1DShadow|sampler2DRectShadow|sampler1DArray|sampler1DArrayShadow|sampler2DArrayShadow|samplerBuffer|sampler2DMS|sampler2DMSArray|samplerCubeShadow|samplerCubeArray|samplerCubeArrayShadow|isampler1D|isampler3D|isamplerCube|isampler2DRect|isampler1DArray|isampler2DArray|isamplerBuffer|isampler2DMS|isampler2DMSArray|isamplerCubeArray|usampler1D|usampler3D|usamplerCube|usampler2DRect|usampler1DArray|usampler2DArray|usamplerBuffer|usampler2DMS|usampler2DMSArray|usamplerCubeArray|dmat2x2|dmat2x3|dmat2x4|dmat3x2|dmat3x3|dmat3x4|dmat4x2|dmat4x3|dmat4x4|subroutine|patch|sample|coherent|restrict|readonly|writeonly|atomic_uint","|");
    i = 0; while i < extra.size() { if name == extra[i] { return false; } i = i+1; } return true;
}
fn ss_graph_add(g :: SSGraph,id :: Int64,kind :: Int64,type_id :: Int64,name :: String,parameter_id :: Int64) -> Int64 {
    s :: Int64 := g.state[0]; if s >= 64 || id < 1 || id > 1000000 || ss_graph_slot(g,id) != -1 || kind < 0 || kind > 9 || type_id < 1 || type_id > 5 || ((kind == 1 || type_id == 5) && (!ss_identifier(name) || parameter_id < 1)) || (kind == 0 && type_id == 5) { throw "Invalid typed graph node"; }
    if kind == 1 && !ss_graph_exposed_name(name) { throw "Reserved/colliding exposed name at graph node "+string(id); }
    se_text_write(g.names[s],name); g.meta[s*8] = id; g.meta[s*8+1] = kind; g.meta[s*8+2] = type_id; g.meta[s*8+3] = -1; g.meta[s*8+4] = -1; g.meta[s*8+5] = -1; g.meta[s*8+6] = parameter_id; g.state[0] = s+1; g.state[1] = g.state[1]+1; return s;
}
fn ss_graph_connect(g :: SSGraph,to_id :: Int64,input :: Int64,from_id :: Int64) -> Bool { s :: Int64 := ss_graph_slot(g,to_id); if s < 0 || input < 0 || input > 2 || (from_id != -1 && ss_graph_slot(g,from_id) < 0) { throw "Unknown graph node/socket"; } g.meta[s*8+3+input] = from_id; g.state[1] = g.state[1]+1; return true; }
fn ss_graph_value(g :: SSGraph,id :: Int64,component :: Int64,value :: Float64) -> Bool { s :: Int64 := ss_graph_slot(g,id); if s < 0 || component < 0 || component > 5 || !(vmath.abs(value) <= 1000000.0) { throw "Invalid graph value/position"; } g.values[s*6+component] = value; g.state[1] = g.state[1]+1; return true; }
fn ss_graph_inputs(kind :: Int64) -> Int64 { if kind == 4 { return 3; } if kind == 2 || kind == 3 || kind == 6 || kind == 8 || kind == 9 { return 2; } if kind == 7 { return 1; } return 0; }
fn ss_graph_visit(g :: SSGraph,s :: Int64,depth :: Int64) -> Bool {
    if depth >= 64 || g.work[s] == 1 { throw "Typed graph cycle at node "+string(g.meta[s*8]); } if g.work[s] == 2 { return true; } g.work[s] = 1;
    kind :: Int64 := g.meta[s*8+1]; type_id :: Int64 := g.meta[s*8+2]; count :: Int64 := ss_graph_inputs(kind); types :: Array<Int64> := [0,0,0]; j :: Int64 := 0;
    while j < 3 { id :: Int64 := g.meta[s*8+3+j]; if j < count { a :: Int64 := ss_graph_slot(g,id); if a < 0 { throw "Missing typed graph input at node "+string(g.meta[s*8]); } if g.meta[a*8+1] == 7 { throw "Fragment output cannot feed another node"; } ss_graph_visit(g,a,depth+1); types[j] = g.meta[a*8+2]; } else { if id != -1 { throw "Graph node has an unused input"; } } j = j+1; }
    if kind == 5 && type_id != 2 { throw "UV output must be vec2"; }
    if kind == 7 && (types[0] != 4 || type_id != 4) { throw "Fragment output requires vec4"; }
    if kind == 6 && (types[0] != 5 || types[1] != 2 || type_id != 4) { throw "Texture sample requires sampler2D and vec2"; }
    if kind == 4 && (types[0] != type_id || types[1] != type_id || type_id == 5 || (types[2] != 1 && types[2] != type_id)) { throw "Mix socket type mismatch"; }
    if kind == 2 || kind == 3 || kind == 8 || kind == 9 { if type_id == 5 || types[0] == 5 || types[1] == 5 || (types[0] != type_id && types[0] != 1) || (types[1] != type_id && types[1] != 1) || (types[0] == 1 && types[1] == 1 && type_id != 1) { throw "Arithmetic socket type mismatch"; } }
    g.work[s] = 2; return true;
}
fn ss_graph_validate(g :: SSGraph) -> Bool {
    i :: Int64 := 0; while i < 64 { g.work[i] = 0; i = i+1; } outputs :: Int64 := 0; i = 0;
    while i < g.state[0] { ss_graph_visit(g,i,0); if g.meta[i*8+1] == 7 { outputs = outputs+1; } if g.meta[i*8+1] == 1 { if !ss_graph_exposed_name(se_text_read(g.names[i])) { throw "Reserved/colliding exposed name at graph node "+string(g.meta[i*8]); } j :: Int64 := 0; while j < i { if g.meta[j*8+1] == 1 && (g.meta[j*8+6] == g.meta[i*8+6] || se_text_read(g.names[j]) == se_text_read(g.names[i])) { throw "Duplicate exposed graph parameter"; } j = j+1; } } i = i+1; }
    if outputs != 1 { throw "Typed graph requires exactly one fragment output"; } return true;
}
fn ss_graph_type(type_id :: Int64) -> String { if type_id == 1 { return "float"; } if type_id == 2 { return "vec2"; } if type_id == 3 { return "vec3"; } if type_id == 4 { return "vec4"; } return "sampler2D"; }
fn ss_graph_gltype(type_id :: Int64) -> Int64 { if type_id == 1 { return 5126; } if type_id == 2 { return 35664; } if type_id == 3 { return 35665; } if type_id == 4 { return 35666; } return 35678; }
fn ss_graph_symbol(id :: Int64) -> String { return "n_"+string(id); }
fn ss_graph_order(g :: SSGraph) -> Bool {
    i :: Int64 := 0; while i < 64 { g.work[i] = 0; i = i+1; } rank :: Int64 := 0;
    while rank < g.state[0] { best :: Int64 := -1; i = 0; while i < g.state[0] { if g.work[i] == 0 { ready :: Bool := true; j :: Int64 := 0; while j < ss_graph_inputs(g.meta[i*8+1]) { a :: Int64 := ss_graph_slot(g,g.meta[i*8+3+j]); if a < 0 || g.work[a] == 0 { ready = false; } j = j+1; } if ready { if best == -1 { best = i; } else { if g.meta[i*8] < g.meta[best*8] { best = i; } } } } i = i+1; } if best == -1 { throw "Typed graph cycle"; } g.work[best] = 1; g.work[64+rank] = best; rank = rank+1; } return true;
}
fn ss_graph_glsl(g :: SSGraph) -> String {
    ss_graph_validate(g); ss_graph_order(g); b :: Int64 := vcore.sb_create(); vcore.sb_append(b,"#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\n"); rank :: Int64 := 0;
    while rank < g.state[0] { s :: Int64 := g.work[64+rank]; if g.meta[s*8+1] == 1 { vcore.sb_append(b,"#line 1 "+string(g.meta[s*8])+"\nuniform "+ss_graph_type(g.meta[s*8+2])+" "+se_text_read(g.names[s])+";\n"); } rank = rank+1; }
    vcore.sb_append(b,"#line 1 0\nvoid main()\{\n"); rank = 0; while rank < g.state[0] { s :: Int64 := g.work[64+rank]; kind :: Int64 := g.meta[s*8+1]; type_id :: Int64 := g.meta[s*8+2]; id :: Int64 := g.meta[s*8]; a :: String := ss_graph_symbol(g.meta[s*8+3]); c :: String := ss_graph_symbol(g.meta[s*8+4]); e :: String := ss_graph_symbol(g.meta[s*8+5]);
        vcore.sb_append(b,"#line 1 "+string(id)+"\n");
        if kind == 7 { vcore.sb_append(b,"finalColor="+a+";\n"); } else { if type_id != 5 { vcore.sb_append(b,ss_graph_type(type_id)+" "+ss_graph_symbol(id)+"="); }
            if kind == 0 { vcore.sb_append(b,ss_graph_type(type_id)+"("); j :: Int64 := 0; while j < type_id { if j > 0 { vcore.sb_append(b,","); } vcore.sb_append(b,ss_glsl_number(g.values[s*6+j])); j = j+1; } vcore.sb_append(b,")"); }
            if kind == 1 { if type_id == 5 { vcore.sb_append(b,"// sampler "+se_text_read(g.names[s])); } else { vcore.sb_append(b,se_text_read(g.names[s])); } }
            if kind == 5 { vcore.sb_append(b,"fragTexCoord"); }
            if kind == 6 { sampler_slot :: Int64 := ss_graph_slot(g,g.meta[s*8+3]); if g.meta[sampler_slot*8+1] != 1 { throw "Sampler must be an exposed texture parameter"; } vcore.sb_append(b,"texture("+se_text_read(g.names[sampler_slot])+","+c+")"); }
            if kind == 4 { vcore.sb_append(b,"mix("+a+","+c+","+e+")"); }
            if kind == 2 { vcore.sb_append(b,a+"+"+c); } if kind == 3 { vcore.sb_append(b,a+"*"+c); } if kind == 8 { vcore.sb_append(b,a+"-"+c); } if kind == 9 { vcore.sb_append(b,a+"/"+c); }
            vcore.sb_append(b,";\n");
        } rank = rank+1;
    } vcore.sb_append(b,"}\n"); return vcore.sb_build(b);
}
fn ss_graph_generate(g :: SSGraph,d :: SSDocument) -> Bool {
    success :: Bool := false; @bounded<33554432> region graph_generate { try {
        text :: String := ss_graph_glsl(g); candidate :: SSDocument := ss_document(); ss_parameters_copy(candidate.parameters,d.parameters); i :: Int64 := 0;
        while i < g.state[0] { if g.meta[i*8+1] == 1 { id :: Int64 := g.meta[i*8+6]; gl :: Int64 := ss_graph_gltype(g.meta[i*8+2]); s :: Int64 := ss_parameter_slot(candidate.parameters,id);
            if s < 0 { ss_parameter_define(candidate,id,se_text_read(g.names[i]),gl,1,g.values[i*6],-1000000.0,1000000.0,"Graph","",ss_continuous(gl),false); s = ss_parameter_slot(candidate.parameters,id); j :: Int64 := 0; while j < candidate.parameters.meta[s*8+3] { candidate.parameters.values[s*64+j] = g.values[i*6+j]; candidate.parameters.defaults[s*64+j] = g.values[i*6+j]; j = j+1; } }
            else { if candidate.parameters.meta[s*8+1] != gl || candidate.parameters.meta[s*8+2] != 1 || se_text_read(candidate.parameters.names[s]) != se_text_read(g.names[i]) { throw "Graph exposed parameter conflicts with document schema"; } }
        } i = i+1; }
        ss_parameters_validate(candidate.parameters); se_text_write(candidate.source,text); se_text_copy(d.source,candidate.source); se_text_copy(g.source,candidate.source); ss_parameters_copy(d.parameters,candidate.parameters); ss_document_source_change(d); d.state[8] = d.state[8]+1; g.state[2] = g.state[1]; g.log[0] = 0; success = true;
    } catch (error) { se_text_write(g.log,string(error)); } }; return success;
}
fn ss_graph_text(g :: SSGraph) -> String { b :: Int64 := vcore.sb_create(); vcore.sb_append(b,"VMGE_GRAPH|1\n"); i :: Int64 := 0; while i < g.state[0] { vcore.sb_append(b,"N"); j :: Int64 := 0; while j < 7 { vcore.sb_append(b,"|"+string(g.meta[i*8+j])); j = j+1; } vcore.sb_append(b,"|"+se_text_hex(g.names[i])); j = 0; while j < 6 { vcore.sb_append(b,"|"+ss_encode(g.values[i*6+j])); j = j+1; } vcore.sb_append(b,"\n"); i = i+1; } return vcore.sb_build(b); }
fn ss_graph_parse(g :: SSGraph,text :: String) -> Bool {
    if text.size() > 65536 { throw "Graph resource budget"; } candidate :: SSGraph := ss_graph(); lines :: Array := ss_split(text,"\n"); if lines[0] != "VMGE_GRAPH|1" { throw "Unsupported graph version"; } row :: Int64 := 1;
    while row < lines.size() { if lines[row] != "" { f :: Array := ss_split(lines[row],"|"); if f.size() != 15 || f[0] != "N" { throw "Invalid graph record"; } name :: Array<Int64> := ss_text(127); ss_unhex(name,f[8]); s :: Int64 := ss_graph_add(candidate,ss_integer(f[1]),ss_integer(f[2]),ss_integer(f[3]),se_text_read(name),ss_integer(f[7])); j :: Int64 := 0; while j < 3 { candidate.meta[s*8+3+j] = ss_integer(f[4+j]); j = j+1; } j = 0; while j < 6 { candidate.values[s*6+j] = ss_number(f[9+j]); j = j+1; } } row = row+1; }
    ss_graph_validate(candidate); i :: Int64 := 0; while i < 512 { g.meta[i] = candidate.meta[i]; i = i+1; } i = 0; while i < 384 { g.values[i] = candidate.values[i]; i = i+1; } i = 0; while i < 64 { se_text_copy(g.names[i],candidate.names[i]); i = i+1; } g.state[0] = candidate.state[0]; g.state[1] = g.state[1]+1; return true;
}
fn ss_graph_save(g :: SSGraph,path :: String) -> Bool { @bounded<16777216> region graph_save { ss_graph_validate(g); if !vfs.write_atomic(path,ss_graph_text(g)) { throw "Cannot save graph"; } }; return true; }
fn ss_graph_load(g :: SSGraph,path :: String) -> Bool { @bounded<16777216> region graph_load { raw := vfs.read(path); if raw == null { throw "Cannot load graph"; } ss_graph_parse(g,raw); }; return true; }
