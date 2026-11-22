use "assets.vy";
use "../shader/pipeline.vy";

# Four live + four reusable staging documents; one camera target bank. All
# source bytes and descriptors are preframe-owned, never committed during edits.
interface SECameraEffects {
    pipeline :: SSPipeline, candidate :: SSPipeline, documents :: Array,
    ids :: Array<Int64>, paths :: Array, state :: Array<Int64>,
    path :: Array<Int64>, attempt :: Array<Int64>, log :: Array<Int64>, values :: Array<Float64>,
}
fn se_camera_effects() -> SECameraEffects {
    docs :: Array := []; paths :: Array := []; i :: Int64 := 0;
    while i < 8 { docs.push(ss_document_with_limits(8191,511,16)); paths.push(ss_text(2048)); i = i+1; }
    return SECameraEffects(ss_pipeline(),ss_pipeline(),docs,ss_ints(8),paths,ss_ints(8),ss_text(2048),ss_text(2048),ss_text(3500),ss_floats(2));
}
fn se_camera_effects_close(e :: SECameraEffects) -> Bool {
    bank :: Array<Int64> := se_camera_effects_bank(e); owner :: vraylib.App := vraylib.App(e.state[2]);
    if bank.size() != 0 && owner.valid() { if !owner.can_release(bank) { throw "Camera bank is active or externally referenced"; } owner.release_bank(bank); }
    i :: Int64 := 0; while i < 16 { e.pipeline.tokens[i] = 0; e.pipeline.outputs[i] = 0; i = i+1; }
    e.pipeline.state[2] = 0; e.state[1] = 0; e.state[3] = 0; ss_pipeline_close(e.candidate);
    i = 0; while i < 8 { ss_document_close(e.documents[i]); ss_document_collect(e.documents[i]); e.ids[i] = 0; i = i+1; } return true;
}
fn se_camera_effects_reload(e :: SECameraEffects) -> Bool { e.state[4] = 1; e.state[6] = 0; return true; }
fn se_camera_effects_budget(p :: SSPipeline,width :: Int64,height :: Int64) -> Int64 {
    if width < 1 || height < 1 || width > 2048 || height > 2048 || p.state[0] < 1 || p.state[0] > 8 { throw "Camera budget: 2048 per axis / eight passes"; }
    bytes :: Int64 := width*height*20; i :: Int64 := 0;
    while i < p.state[0] {
        w :: Int64 := p.meta[i*10+5]; h :: Int64 := p.meta[i*10+6]; if w == 0 { w = width/p.meta[i*10+7]; } if h == 0 { h = height/p.meta[i*10+7]; }
        w = int64(vmath.max(1.0,float64(w))); h = int64(vmath.max(1.0,float64(h)));
        if w > 2048 || h > 2048 { throw "Camera pass exceeds 2048 per axis"; }
        pixel :: Int64 := 8; if p.meta[i*10+8] == 1 { pixel = 20; } bytes = bytes+w*h*pixel; i = i+1;
    } return bytes;
}
fn se_camera_effects_bank(e :: SECameraEffects) -> Array<Int64> {
    bank :: Array<Int64> := ss_pipeline_compact_bank(e.pipeline.tokens); if e.state[1] != 0 { bank.push(e.state[1]); } return bank;
}
fn se_camera_effects_prepare(app :: vraylib.App,e :: SECameraEffects,root :: String,path :: String,width :: Int64,height :: Int64) -> Bool {
    full :: String := path; if !vraylib.is_path_absolute(full) { full = vfs.join(root,full); } full = ss_normal_path(full);
    changed :: Bool := se_text_read(e.path) != full || e.state[4] != 0;
    if changed && e.state[4] == 0 && se_text_read(e.attempt) == full { return e.state[1] != 0; }
    if !changed && e.state[6] == width*8192+height && e.pipeline.state[2] == e.pipeline.state[1] { return e.state[1] != 0; }
    if !changed && e.state[5] == width*8192+height && e.pipeline.state[2] == e.pipeline.state[1] { return true; }
    success :: Bool := false; stage :: Int64 := 4-e.state[0]; loaded :: Int64 := 0;
    @bounded<67108864> region camera_pipeline_stage {
        new_input :: Int64 := 0;
        c :: SSPipeline := e.candidate; old_bank :: Array<Int64> := se_camera_effects_bank(e); stage_ids :: Array<Int64> := ss_ints(4);
        try {
            if app.passes() != 0 { throw "Camera preparation requires no active passes"; }
            if old_bank.size() != 0 && !app.can_release(old_bank) { throw "Camera outputs are still externally referenced"; }
            ss_pipeline_close(c);
            if changed {
                se_text_write(e.attempt,full); e.state[4] = 0;
                raw := vfs.read(full+".host"); if raw == null { raw = vfs.read(full); } if raw == null { throw "Cannot read camera pipeline: "+full; }
                if raw.size() > 16384 { throw "Camera pipeline ledger exceeds 16 KiB"; }
                rows :: Array := ss_split(raw,"\n"); if ss_trim(rows[0]) != "VMGE_RENDER_PIPELINE|1" { throw "Camera pipeline requires a Rendering document ledger (.host)"; }
                builder :: Int64 := vcore.sb_create(); row :: Int64 := 1; count :: Int64 := 0;
                while row < rows.size() { line :: String := ss_trim(rows[row]); if line != "" && line[0] == "D" {
                    f :: Array := ss_split(line,"|"); if f.size() != 3 || count >= 4 { throw "Camera shader document budget: four"; }
                    id :: Int64 := ss_integer(f[1]); if id < 1 { throw "Invalid camera document ID"; }
                    i :: Int64 := 0; while i < count { if stage_ids[i] == id { throw "Duplicate camera document ID"; } i = i+1; }
                    stage_ids[count] = id; ss_unhex(e.paths[stage+count],f[2]); resource :: String := se_text_read(e.paths[stage+count]);
                    if !vraylib.is_path_absolute(resource) { resource = vfs.join(root,resource); } se_text_write(e.paths[stage+count],ss_normal_path(resource)); count = count+1;
                } else { vcore.sb_append(builder,line+"\n"); } row = row+1; }
                ss_pipeline_parse(c,vcore.sb_build(builder));
                i :: Int64 := 0; while i < c.state[0] { found :: Bool := false; j :: Int64 := 0; while j < count { if stage_ids[j] == c.meta[i*10+1] { found = true; } j = j+1; } if !found { throw "Missing camera shader resource in ledger"; } i = i+1; }
                i = 0; while i < count {
                    d :: SSDocument := e.documents[stage+i]; ss_document_close(d); ss_document_collect(d); loaded = i+1;
                    ss_document_load(d,se_text_read(e.paths[stage+i])); if !ss_compile(app,d) { throw "Camera shader failed: "+se_text_read(d.log); } i = i+1;
                }
            } else {
                i :: Int64 := 0; while i < 160 { c.meta[i] = e.pipeline.meta[i]; i = i+1; } c.state[0] = e.pipeline.state[0]; c.state[1] = c.state[1]+1;
            }
            bytes :: Int64 := se_camera_effects_budget(c,width,height);
            if bytes+e.state[3] > 134217728 { throw "Camera old + candidate GPU budget exceeds 128 MiB (including estimated depth)"; }
            input :: vraylib.RenderTarget := app.float_target(width,height); new_input = input.token; ss_pipeline_prepare(app,c,width,height);
            se_camera_candidate_validate(app,e,c,stage_ids,stage,changed,input);
            if old_bank.size() != 0 { app.release_bank(old_bank); }
            # Publication is numeric transfer into existing owners only.
            i :: Int64 := 0; while i < 160 { e.pipeline.meta[i] = c.meta[i]; i = i+1; }
            i = 0; while i < 5 { e.pipeline.state[i] = c.state[i]; i = i+1; }
            i = 0; while i < 16 { e.pipeline.tokens[i] = c.tokens[i]; e.pipeline.outputs[i] = 0; c.tokens[i] = 0; c.outputs[i] = 0; i = i+1; } c.state[2] = 0;
            e.state[1] = new_input; new_input = 0; e.state[2] = app.token; e.state[3] = bytes; e.state[5] = width*8192+height;
            if changed {
                i = 0; while i < 4 { old :: SSDocument := e.documents[e.state[0]+i]; ss_document_close(old); ss_document_collect(old); e.ids[e.state[0]+i] = 0; e.ids[stage+i] = stage_ids[i]; i = i+1; }
                e.state[0] = stage; se_text_write(e.path,full);
            } e.log[0] = 0; e.state[6] = 0; success = true;
        } catch (error) { message :: String := string(error); if message.size() < 3500 { se_text_write(e.log,message); } else { se_text_write(e.log,"Camera pipeline failed; last working pipeline retained"); } out(se_text_read(e.log)); }
        finally { if !success {
            e.state[6] = width*8192+height;
            if new_input != 0 { vraylib.RenderTarget(new_input,app.token).close(); }
            ss_pipeline_close(c); i :: Int64 := 0; while i < loaded { ss_document_close(e.documents[stage+i]); ss_document_collect(e.documents[stage+i]); i = i+1; }
        } }
    }; return success || e.state[1] != 0;
}
fn se_camera_effect_set(e :: SECameraEffects,document_id :: Int64,parameter_id :: Int64,component :: Int64,value :: Float64) -> Bool {
    if document_id < 1 || e.state[1] == 0 { throw "Camera effect requires a prepared owner and positive document ID"; }
    i :: Int64 := e.state[0]; while i < e.state[0]+4 { if e.ids[i] == document_id { return ss_parameter_set(e.documents[i],parameter_id,component,value); } i = i+1; } throw "Unknown camera effect document";
}
fn se_camera_candidate_validate(app :: vraylib.App,e :: SECameraEffects,p :: SSPipeline,ids :: Array<Int64>,stage :: Int64,changed :: Bool,input :: vraylib.RenderTarget) -> Bool {
    i :: Int64 := 0; while i < p.state[0] { if p.meta[i*10+4] != 0 {
        slot :: Int64 := -1; j :: Int64 := 0; while j < 4 {
            if changed { if ids[j] == p.meta[i*10+1] { slot = stage+j; } }
            else { if e.ids[e.state[0]+j] == p.meta[i*10+1] { slot = e.state[0]+j; } } j = j+1;
        }
        if slot == -1 { throw "Missing camera pass document"; }
        d :: SSDocument := e.documents[slot]; shader :: vraylib.Shader := ss_program(d); flags :: Int64 := ss_pipeline_sampler_flags(shader);
        if flags%2 != 1 || (p.meta[i*10+3] == -2 && flags/2 != 0) || (p.meta[i*10+3] != -2 && flags/2 != 1) { throw "Camera pass controlled sampler mismatch"; }
        ss_material_validate_for(d,p.defaults,true); ss_pipeline_engine_inputs(app,p,shader,input,vraylib.RenderTarget(p.tokens[i],app.token),false);
    } i = i+1; } return true;
}
fn se_camera_effects_run(app :: vraylib.App,e :: SECameraEffects) -> vraylib.RenderTarget {
    e.values[0] = e.values[0]+app.dt(); e.pipeline.context[0] = e.values[0]; e.pipeline.context[1] = app.dt(); e.pipeline.context[2] = 1.0;
    return ss_pipeline_run(app,e.pipeline,e.documents,e.ids,vraylib.RenderTarget(e.state[1],app.token));
}
fn se_camera_authored_view(scene :: SEScene,cache :: SETransforms,camera :: vraylib.Camera) -> Bool {
    id :: Int64 := se_active_node(scene,2); if id == -1 { return false; }
    matrix :: vraylib.Matrix := se_world(scene,cache,id); eye :: vraylib.Vector3 := vraylib.math_vector3_transform(vraylib.vec3(0.0,0.0,0.0),matrix);
    target :: vraylib.Vector3 := vraylib.math_vector3_transform(vraylib.vec3(0.0,0.0,1.0),matrix); camera.set_view(eye,target);
    view :: vraylib.Camera3D := camera.view(); view.fovy = scene.number(id,13); vraylib.buffer_write(vraylib.require_managed(camera.session,camera.token),0,view); return true;
}
fn se_scene_capture(app :: vraylib.App,scene :: SEScene,cache :: SETransforms,bank :: SEAssets,cubes :: Array,camera :: vraylib.Camera,target :: vraylib.RenderTarget) -> Bool {
    se_environment_prepare(scene,cache,bank);
    eye :: vraylib.Vector3 := camera.position(); bank.view[0] = eye.x; bank.view[1] = eye.y; bank.view[2] = eye.z;
    target.begin(); try {
        app.clear(vraylib.rgba(43,48,58,255));
        if bank.environment_tokens[1] != 0 { se_sky_draw(app,scene,camera,vraylib.Shader(bank.environment_tokens[1],app.token),bank.lights,target.width(),target.height()); }
        camera.begin(); try { se_draw_scene(scene,cache,bank,cubes,false,-1); } finally { camera.end(); }
    } finally { target.end(); } return true;
}
