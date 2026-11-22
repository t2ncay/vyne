use "preprocess.vy";
use external "vraylib/Managed.vy";

fn ss_program(d :: SSDocument) -> vraylib.Shader { return vraylib.Shader(d.state[4],d.state[5]); }
fn ss_document_collect(d :: SSDocument) -> Bool {
    i :: Int64 := 0; kept :: Int64 := 0; while i < d.state[12] { shader :: vraylib.Shader := vraylib.Shader(d.retired[i],d.retired_sessions[i]); blocked :: Bool := false; try { if shader.valid() { shader.close(); } } catch (error) { blocked = true; }
        if blocked { d.retired[kept] = d.retired[i]; d.retired_sessions[kept] = d.retired_sessions[i]; kept = kept+1; } i = i+1;
    } d.state[12] = kept; return kept == 0;
}
fn ss_document_retire(d :: SSDocument,shader :: vraylib.Shader) -> Bool {
    if shader.token == 0 || !shader.valid() { return true; } try { shader.close(); return true; } catch (error) {
        n :: Int64 := d.state[12]; if n >= 16 { throw "Deferred shader release capacity"; } d.retired[n] = shader.token; d.retired_sessions[n] = shader.session; d.state[12] = n+1; ss_document_log(d,se_text_read(d.log)+"\nPrevious shader still referenced; release deferred until dependencies detach.");
    } return false;
}
fn ss_document_close(d :: SSDocument) -> Bool { ss_document_collect(d); if d.state[4] == 0 { return false; } if d.state[12] >= 16 { throw "Detach shader dependencies before closing document"; } ss_document_retire(d,ss_program(d)); d.state[4] = 0; d.state[2] = 0; d.state[3] = 0; return true; }
fn ss_reflect(p :: SSParameters,rows :: Array) -> Bool {
    i :: Int64 := 0; while i < p.state[0] { p.meta[i*8+6] = 2; p.meta[i*8+7] = -1; j :: Int64 := 0;
        while j < rows.size() { row :: Array := rows[j]; if row.size() != 4 { throw "Invalid runtime reflection row"; } name :: String := row[0]; declared :: String := se_text_read(p.names[i]);
            if name == declared || name == declared+"[0]" { if int64(row[1]) == p.meta[i*8+1] && int64(row[2]) == p.meta[i*8+2] && int64(row[3]) >= 0 && ss_upload_supported(p.meta[i*8+1]) && (p.meta[i*8+1] != 35678 || (p.meta[i*8+2] == 1 && name == declared)) { p.meta[i*8+6] = 1; p.meta[i*8+7] = int64(row[3]); } else { p.meta[i*8+6] = 3; } }
            j = j+1;
        } i = i+1;
    } return true;
}
fn ss_compile(app :: vraylib.App,d :: SSDocument) -> Bool {
    success :: Bool := false;
    @bounded<67108864> region shader_build {
        candidate_token :: Int64 := 0;
        try {
            ss_document_collect(d); if d.state[12] >= 16 { throw "Detach old shader dependencies before compiling again (16 deferred programs)"; }
            ss_parameters_validate(d.parameters);
            prep :: SSPreprocessor := ss_preprocessor(); vp :: String := se_text_read(d.vertex_path); fp :: String := se_text_read(d.fragment_path); if vp == "" { vp = se_text_read(d.path); } if fp == "" { fp = se_text_read(d.path); }
            vertex :: String := "#version 330\nin vec3 vertexPosition;\nin vec2 vertexTexCoord;\nin vec4 vertexColor;\nout vec2 fragTexCoord;\nout vec4 fragColor;\nuniform mat4 mvp;\nvoid main()\{fragTexCoord=vertexTexCoord;fragColor=vertexColor;gl_Position=mvp*vec4(vertexPosition,1.0);}\n";
            if d.vertex[0] > 0 { vertex = ss_expand(prep,se_text_read(d.vertex),vp,0,0); } fragment :: String := ss_expand(prep,se_text_read(d.source),fp,0,0);
            build :: vraylib.ShaderBuild := app.compile_shader(vertex,fragment); candidate_token = build.shader.token; ss_document_log(d,build.log);
            if build.success {
                candidate :: SSParameters := ss_parameters(); ss_parameters_copy(candidate,d.parameters); ss_reflect(candidate,build.shader.uniforms());
                diagnostic :: Int64 := vcore.sb_create(); vcore.sb_append(diagnostic,build.log); i :: Int64 := 0; while i < candidate.state[0] { if candidate.meta[i*8+6] != 1 { vcore.sb_append(diagnostic,"\nOrphan parameter "+string(candidate.meta[i*8])+": "+se_text_read(candidate.names[i])); } i = i+1; } ss_document_log(d,vcore.sb_build(diagnostic));
                old :: vraylib.Shader := ss_program(d); ss_parameters_copy(d.parameters,candidate); ss_parameters_copy(d.published,candidate); i = 0; while i < prep.state[0] { se_text_copy(d.dependencies[i],prep.paths[i]); d.dependency_hashes[i] = prep.hashes[i]; i = i+1; } d.state[10] = prep.state[0];
                d.state[4] = candidate_token; d.state[5] = app.token; d.state[2] = d.state[11]; d.state[3] = 0; d.state[13] = ss_hash(d.source); d.state[14] = ss_hash(d.vertex); candidate_token = 0; success = true;
                ss_document_retire(d,old);
            }
        } catch (error) { ss_document_log(d,string(error)); }
        if candidate_token != 0 { vraylib.Shader(candidate_token,app.token).close(); }
    }; return success;
}
