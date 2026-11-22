use "material.vy";

# Ordered pass stride 10: pass ID, document ID, input0 ID (-1 external), input1 ID
# (-2 absent), enabled, width (0 relative), height (0 relative), divisor, format
# (0 RGBA8, 1 RGBA32F), capture. Output resource ID is the pass ID.
interface SSPipeline { meta :: Array<Int64>, state :: Array<Int64>, tokens :: Array<Int64>, outputs :: Array<Int64>, work :: Array<Int64>, defaults :: MaterialInstance, context :: Array<Float64> }
fn ss_pipeline() -> SSPipeline { return SSPipeline(ss_ints(160),ss_ints(5),ss_ints(16),ss_ints(16),ss_ints(32),ss_material(1,1),ss_floats(3)); }
fn ss_pipeline_slot(p :: SSPipeline,id :: Int64) -> Int64 { i :: Int64 := 0; while i < p.state[0] { if p.meta[i*10] == id { return i; } i = i+1; } return -1; }
fn ss_pipeline_add(p :: SSPipeline,id :: Int64,document_id :: Int64,input0 :: Int64,input1 :: Int64,width :: Int64,height :: Int64,divisor :: Int64,format :: Int64) -> Int64 {
    s :: Int64 := p.state[0]; if s >= 16 || id < 1 || document_id < 1 || ss_pipeline_slot(p,id) != -1 || width < 0 || height < 0 || width > 4096 || height > 4096 || divisor < 1 || divisor > 16 || format < 0 || format > 1 { throw "Invalid pipeline pass/target"; }
    p.meta[s*10] = id; p.meta[s*10+1] = document_id; p.meta[s*10+2] = input0; p.meta[s*10+3] = input1; p.meta[s*10+4] = 1; p.meta[s*10+5] = width; p.meta[s*10+6] = height; p.meta[s*10+7] = divisor; p.meta[s*10+8] = format; p.meta[s*10+9] = 0; p.state[0] = s+1; p.state[1] = p.state[1]+1; return s;
}
fn ss_pipeline_enable(p :: SSPipeline,id :: Int64,enabled :: Bool,capture :: Bool) -> Bool { s :: Int64 := ss_pipeline_slot(p,id); if s < 0 { throw "Unknown pipeline pass"; } p.meta[s*10+4] = int64(enabled); p.meta[s*10+9] = int64(capture); p.state[1] = p.state[1]+1; return true; }
fn ss_pipeline_visit(p :: SSPipeline,s :: Int64,depth :: Int64) -> Bool {
    if depth >= 16 || p.work[s] == 1 { throw "Pipeline cycle"; } if p.work[s] == 2 { return true; } p.work[s] = 1;
    j :: Int64 := 2; while j <= 3 { id :: Int64 := p.meta[s*10+j]; if id >= 0 { input :: Int64 := ss_pipeline_slot(p,id); if input < 0 { throw "Missing pipeline input"; } if input == s { throw "Pipeline write feedback"; } ss_pipeline_visit(p,input,depth+1); } else { if (j == 2 && id != -1) || (j == 3 && id != -1 && id != -2) { throw "Invalid pipeline external input"; } } j = j+1; } p.work[s] = 2; return true;
}
fn ss_pipeline_validate(p :: SSPipeline) -> Bool {
    if p.state[0] < 0 || p.state[0] > 16 { throw "Invalid pipeline pass count"; }
    i :: Int64 := 0; while i < p.state[0] { b :: Int64 := i*10;
        if p.meta[b] < 1 || p.meta[b+1] < 1 || p.meta[b+4] < 0 || p.meta[b+4] > 1 || p.meta[b+5] < 0 || p.meta[b+5] > 4096 || p.meta[b+6] < 0 || p.meta[b+6] > 4096 || p.meta[b+7] < 1 || p.meta[b+7] > 16 || p.meta[b+8] < 0 || p.meta[b+8] > 1 || p.meta[b+9] < 0 || p.meta[b+9] > 1 { throw "Invalid pipeline target descriptor/flags"; }
        j :: Int64 := 0; while j < i { if p.meta[j*10] == p.meta[b] { throw "Duplicate pipeline pass ID"; } j = j+1; } i = i+1;
    }
    i = 0; while i < 16 { p.work[i] = 0; i = i+1; } i = 0; while i < p.state[0] { ss_pipeline_visit(p,i,0); i = i+1; }
    i = 0; while i < p.state[0] { j :: Int64 := 2; while j <= 3 { if p.meta[i*10+j] >= 0 && ss_pipeline_slot(p,p.meta[i*10+j]) >= i { throw "Pipeline dependency must precede consumer"; } j = j+1; } i = i+1; } return true;
}
fn ss_pipeline_bank_present(p :: SSPipeline) -> Bool { i :: Int64 := 0; while i < p.tokens.size() { if p.tokens[i] != 0 { return true; } i = i+1; } return false; }
fn ss_pipeline_compact_bank(tokens :: Array<Int64>) -> Array<Int64> { bank :: Array<Int64> := []; bank.reserve(tokens.size()); i :: Int64 := 0; while i < tokens.size() { if tokens[i] != 0 { bank.push(tokens[i]); } i = i+1; } return bank; }
fn ss_pipeline_candidate_close(app :: vraylib.App,bank :: Array<Int64>) -> Bool {
    if bank.size() == 0 || !app.valid() { return true; }
    try { app.release_bank(bank); } catch (error) {
        # Bank-release allocation can fail before mutation. These fresh candidates
        # have never escaped, been sampled, or entered a pass; single close needs
        # no bank staging allocation and cannot have dependency blockers.
        i :: Int64 := 0; while i < bank.size() { target :: vraylib.RenderTarget := vraylib.RenderTarget(bank[i],app.token); if target.valid() { target.close(); } i = i+1; }
    } return true;
}
fn ss_pipeline_close(p :: SSPipeline) -> Bool {
    if ss_pipeline_bank_present(p) { owner :: vraylib.App := vraylib.App(p.state[3]); if owner.valid() { bank :: Array<Int64> := ss_pipeline_compact_bank(p.tokens); if !owner.can_release(bank) || !owner.release_bank(bank) { throw "Pipeline target bank is active or externally referenced"; } } }
    i :: Int64 := 0; while i < p.tokens.size() { p.tokens[i] = 0; p.outputs[i] = 0; i = i+1; } p.state[2] = 0; return true;
}
fn ss_pipeline_prepare(app :: vraylib.App,p :: SSPipeline,width :: Int64,height :: Int64) -> Bool {
    ss_pipeline_validate(p); if width < 1 || height < 1 || width > 4096 || height > 4096 { throw "Invalid pipeline resolution"; }
    if p.state[2] == p.state[1] && p.state[3] == app.token && p.state[4] == width*8192+height { return false; }
    if !app.valid() || app.passes() != 0 { throw "Pipeline preparation requires a live application with no active passes"; }
    # Preflight the whole old bank before allocating any candidate. No partial close.
    if ss_pipeline_bank_present(p) { owner :: vraylib.App := vraylib.App(p.state[3]); if owner.valid() { bank :: Array<Int64> := ss_pipeline_compact_bank(p.tokens); if !owner.can_release(bank) { throw "Pipeline target bank is active or externally referenced"; } } }
    staged :: Array<Int64> := []; staged.reserve(16); i :: Int64 := 0; published :: Bool := false;
    try {
        while i < p.state[0] { w :: Int64 := p.meta[i*10+5]; h :: Int64 := p.meta[i*10+6]; if w == 0 { w = width/p.meta[i*10+7]; } if h == 0 { h = height/p.meta[i*10+7]; } if w < 1 { w = 1; } if h < 1 { h = 1; }
            target :: vraylib.RenderTarget := vraylib.RenderTarget(0,app.token); if p.meta[i*10+8] == 1 { target = app.float_target(w,h); } else { target = app.target(w,h); } staged.push(target.token); i = i+1;
        }
        ss_pipeline_close(p); i = 0; while i < staged.size() { p.tokens[i] = staged[i]; i = i+1; } p.state[2] = p.state[1]; p.state[3] = app.token; p.state[4] = width*8192+height; published = true;
    } finally { if !published { ss_pipeline_candidate_close(app,staged); } } return true;
}
fn ss_pipeline_input(p :: SSPipeline,id :: Int64,input_target :: vraylib.RenderTarget) -> vraylib.RenderTarget { if id == -1 { return input_target; } s :: Int64 := ss_pipeline_slot(p,id); if s < 0 || p.outputs[s] == 0 { throw "Pipeline input unavailable"; } return vraylib.RenderTarget(p.outputs[s],p.state[3]); }
fn ss_pipeline_reflected_inputs(rows :: Array) -> Int64 {
    flags :: Int64 := 0; i :: Int64 := 0; while i < rows.size() { row :: Array := rows[i]; if row.size() != 4 { throw "Invalid pipeline reflection row"; } name :: String := row[0]; bit :: Int64 := 0;
        if name == "inputTexture" || name == "inputTexture[0]" { bit = 1; } if name == "inputTexture1" || name == "inputTexture1[0]" { bit = 2; }
        builtin :: Int64 := 0; if name == "mvp" || name == "mvp[0]" { builtin = 35676; } if name == "colDiffuse" || name == "colDiffuse[0]" { builtin = 35666; } if name == "texture0" || name == "texture0[0]" { builtin = 35678; }
        if builtin != 0 && int64(row[3]) >= 0 { if int64(row[1]) != builtin || int64(row[2]) != 1 || (name != "mvp" && name != "colDiffuse" && name != "texture0") { throw "Incompatible fullscreen built-in uniform"; } }
        if bit != 0 { if int64(row[1]) != 35678 || int64(row[2]) != 1 || int64(row[3]) < 0 || !ss_controlled_sampler(name) { throw "Pipeline controlled input must be an active scalar sampler2D"; } flags = flags+bit; } i = i+1;
    } return flags;
}
fn ss_pipeline_sampler_flags(shader :: vraylib.Shader) -> Int64 { return ss_pipeline_reflected_inputs(shader.uniforms()); }
fn ss_pipeline_clear_controlled(shader :: vraylib.Shader) -> Bool {
    rows :: Array := shader.uniforms(); i :: Int64 := 0; while i < rows.size() { row :: Array := rows[i]; name :: String := row[0]; if ss_controlled_sampler(name) || name == "inputTexture[0]" || name == "inputTexture1[0]" { if int64(row[3]) >= 0 { shader.set_texture(name,null); } } i = i+1; } return true;
}
fn ss_pipeline_cleanup(app :: vraylib.App,p :: SSPipeline,documents :: Array,document_ids :: Array<Int64>) -> Bool {
    count :: Int64 := p.state[0]; if count < 0 { count = 0; } if count > 16 { count = 16; }
    i :: Int64 := 0; while i < documents.size() && i < document_ids.size() { used :: Bool := false; j :: Int64 := 0; while j < count { if p.meta[j*10+4] == 1 && p.meta[j*10+1] == document_ids[i] { used = true; } j = j+1; }
        if used { d :: SSDocument := documents[i]; shader :: vraylib.Shader := ss_program(d); if shader.valid() && shader.session == app.token { ss_pipeline_clear_controlled(shader); } } i = i+1;
    } return true;
}
fn ss_pipeline_planned_input(p :: SSPipeline,planned :: Array<Int64>,id :: Int64,input_target :: vraylib.RenderTarget) -> Int64 { if id == -1 { return input_target.token; } s :: Int64 := ss_pipeline_slot(p,id); if s < 0 || planned[s] == 0 { throw "Pipeline planned input unavailable"; } return planned[s]; }
fn ss_pipeline_run(app :: vraylib.App,p :: SSPipeline,documents :: Array,document_ids :: Array<Int64>,input_target :: vraylib.RenderTarget) -> vraylib.RenderTarget {
    result :: Int64 := 0; @bounded<16777216> region pipeline_execution {
        try {
            ss_pipeline_validate(p); if p.state[2] != p.state[1] || p.state[3] != app.token || !input_target.valid() || input_target.session != app.token { throw "Pipeline must be prepared for this application"; }
            if app.passes() != 0 { throw "Pipeline fullscreen execution requires no active render passes"; }
            if documents.size() != document_ids.size() || p.state[0] == 0 { throw "Invalid pipeline document registry/empty pipeline"; }
            planned :: Array<Int64> := ss_ints(16); flags :: Array<Int64> := ss_ints(16); i :: Int64 := 0;
            # Resolve all inputs, targets, reflection and numeric values before any draw.
            while i < p.state[0] { p.work[16+i] = -1; a :: Int64 := ss_pipeline_planned_input(p,planned,p.meta[i*10+2],input_target);
                if p.meta[i*10+4] == 0 { planned[i] = a; } else {
                    j :: Int64 := 0; while j < document_ids.size() { if document_ids[j] == p.meta[i*10+1] { if p.work[16+i] != -1 { throw "Duplicate pipeline document ID"; } p.work[16+i] = j; } j = j+1; } if p.work[16+i] == -1 { throw "Missing pipeline document"; }
                    d :: SSDocument := documents[p.work[16+i]]; shader :: vraylib.Shader := ss_program(d); if !shader.valid() || shader.session != app.token { throw "Pipeline program unavailable"; }
                    flags[i] = ss_pipeline_sampler_flags(shader); if flags[i]%2 != 1 || (p.meta[i*10+3] == -2 && flags[i]/2 != 0) || (p.meta[i*10+3] != -2 && flags[i]/2 != 1) { throw "Pipeline controlled sampler descriptor missing/inactive"; }
                     ss_material_validate_for(d,p.defaults,true); target :: vraylib.RenderTarget := vraylib.RenderTarget(p.tokens[i],app.token); if !target.valid() || a == target.token { throw "Pipeline target unavailable/write feedback"; }
                     ss_pipeline_engine_inputs(app,p,shader,vraylib.RenderTarget(a,app.token),target,false);
                    if p.meta[i*10+3] != -2 { b :: Int64 := ss_pipeline_planned_input(p,planned,p.meta[i*10+3],input_target); if b == target.token { throw "Pipeline second-input write feedback"; } } planned[i] = target.token;
                } i = i+1;
            }
            i = 0; while i < p.state[0] { if p.meta[i*10+4] == 1 {
                input :: vraylib.RenderTarget := vraylib.RenderTarget(ss_pipeline_planned_input(p,planned,p.meta[i*10+2],input_target),app.token); target :: vraylib.RenderTarget := vraylib.RenderTarget(planned[i],app.token); d :: SSDocument := documents[p.work[16+i]]; shader :: vraylib.Shader := ss_program(d);
                 ss_pipeline_clear_controlled(shader); ss_material_apply_mode(d,p.defaults,true); shader.set_texture("inputTexture",input.texture());
                 ss_pipeline_engine_inputs(app,p,shader,input,target,true);
                if p.meta[i*10+3] != -2 { second :: vraylib.RenderTarget := vraylib.RenderTarget(ss_pipeline_planned_input(p,planned,p.meta[i*10+3],input_target),app.token); shader.set_texture("inputTexture1",second.texture()); }
                target.draw_from(input,shader); ss_pipeline_clear_controlled(shader);
            } i = i+1; }
            i = 0; while i < p.state[0] { p.outputs[i] = planned[i]; i = i+1; } result = p.outputs[p.state[0]-1];
        } finally { ss_pipeline_cleanup(app,p,documents,document_ids); }
    }; return vraylib.RenderTarget(result,app.token);
}
fn ss_pipeline_capture(p :: SSPipeline,id :: Int64) -> Array<Float64> { s :: Int64 := ss_pipeline_slot(p,id); if s < 0 || p.meta[s*10+9] == 0 || p.outputs[s] == 0 { throw "Intermediate capture is not enabled/available"; } return vraylib.RenderTarget(p.outputs[s],p.state[3]).read_pixels(); }
fn ss_pipeline_text(p :: SSPipeline) -> String { b :: Int64 := vcore.sb_create(); vcore.sb_append(b,"VMGE_PIPELINE|1\n"); i :: Int64 := 0; while i < p.state[0] { vcore.sb_append(b,"P"); j :: Int64 := 0; while j < 10 { vcore.sb_append(b,"|"+string(p.meta[i*10+j])); j = j+1; } vcore.sb_append(b,"\n"); i = i+1; } return vcore.sb_build(b); }
fn ss_pipeline_parse(p :: SSPipeline,text :: String) -> Bool {
    if text.size() > 8192 { throw "Pipeline text budget"; } candidate :: SSPipeline := ss_pipeline(); lines :: Array := ss_split(text,"\n"); if lines[0] != "VMGE_PIPELINE|1" { throw "Unsupported pipeline version"; } row :: Int64 := 1;
    while row < lines.size() { if lines[row] != "" { f :: Array := ss_split(lines[row],"|"); if f.size() != 11 || f[0] != "P" { throw "Invalid pipeline record"; } ss_pipeline_add(candidate,ss_integer(f[1]),ss_integer(f[2]),ss_integer(f[3]),ss_integer(f[4]),ss_integer(f[6]),ss_integer(f[7]),ss_integer(f[8]),ss_integer(f[9])); enabled :: Int64 := ss_integer(f[5]); capture :: Int64 := ss_integer(f[10]); if enabled < 0 || enabled > 1 || capture < 0 || capture > 1 { throw "Invalid pipeline flags"; } ss_pipeline_enable(candidate,ss_integer(f[1]),enabled == 1,capture == 1); } row = row+1; }
    ss_pipeline_validate(candidate); i :: Int64 := 0; while i < 160 { p.meta[i] = candidate.meta[i]; i = i+1; } p.state[0] = candidate.state[0]; p.state[1] = p.state[1]+1; return true;
}
fn ss_pipeline_save(p :: SSPipeline,path :: String) -> Bool { @bounded<16777216> region pipeline_save { ss_pipeline_validate(p); if !vfs.write_atomic(path,ss_pipeline_text(p)) { throw "Cannot save pipeline"; } }; return true; }
fn ss_pipeline_load(p :: SSPipeline,path :: String) -> Bool { @bounded<16777216> region pipeline_load { raw := vfs.read(path); if raw == null { throw "Cannot load pipeline"; } ss_pipeline_parse(p,raw); }; return true; }
fn ss_pipeline_engine_inputs(app :: vraylib.App,p :: SSPipeline,shader :: vraylib.Shader,input :: vraylib.RenderTarget,target :: vraylib.RenderTarget,upload :: Bool) -> Bool {
    rows :: Array := shader.uniforms(); i :: Int64 := 0; while i < rows.size() {
        row :: Array := rows[i]; name :: String := row[0]; expected :: Int64 := 0;
        if name == "seTime" || name == "seDeltaTime" { expected = 5126; }
        if name == "seInputResolution" || name == "seOutputResolution" { expected = 35664; }
        if expected != 0 {
            if int64(row[1]) != expected || int64(row[2]) != 1 || int64(row[3]) < 0 { throw "Incompatible camera engine uniform: "+name; }
            if upload {
                if name == "seTime" { time :: Float64 := vraylib.get_time(); if p.context[2] != 0.0 { time = p.context[0]; } shader.set(name,time); }
                if name == "seDeltaTime" { delta :: Float64 := app.dt(); if p.context[2] != 0.0 { delta = p.context[1]; } shader.set(name,delta); }
                if name == "seInputResolution" { shader.set(name,vraylib.vec2(float64(input.width()),float64(input.height()))); }
                if name == "seOutputResolution" { shader.set(name,vraylib.vec2(float64(target.width()),float64(target.height()))); }
            }
        } i = i+1;
    } return true;
}
