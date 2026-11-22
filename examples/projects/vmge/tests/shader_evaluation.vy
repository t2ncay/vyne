use "../src/shader/evaluation.vy";
gpu_document :: SSDocument := ss_document();
gpu_evaluation :: SSEvaluation := ss_evaluation(8,8);
gpu_context :: Array<Int64> := [0,0];
rollback_evaluation :: SSEvaluation := ss_evaluation(1,1);
rollback_control :: Array<Int64> := [0,0];
gpu_rollback_control :: Array<Int64> := [0,0];
fn ev_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
fn ev_objective(m :: MaterialInstance) -> Float64 { x :: Float64 := ss_material_get(m,1,0)-0.8; return x*x; }
fn ev_failure(m :: MaterialInstance) -> Float64 { throw "Intentional render failure"; }
fn ev_final_failure(m :: MaterialInstance) -> Float64 {
    rollback_control[1] = rollback_control[1]+1; if rollback_control[1] == 3 {
        rollback_evaluation.reference[0] = 99.0; rollback_evaluation.difference[0] = 98.0; rollback_evaluation.metrics[0] = 97.0; rollback_evaluation.context[0] = 96.0; rollback_evaluation.gradients[0] = 95.0; rollback_evaluation.optimizer.params[0] = 94.0; rollback_evaluation.state[3] = 93;
        if rollback_control[0] == 0 { throw "Final candidate failed"; } return float64("nan");
    } return ev_objective(m);
}
fn ev_camera() -> vraylib.Camera3D { return vraylib.Camera3D(vraylib.vec3(0.0,0.0,4.0),vraylib.vec3(0.0,0.0,0.0),vraylib.vec3(0.0,1.0,0.0),60.0,0); }
fn ev_gpu_objective(m :: MaterialInstance) -> Float64 {
    app :: vraylib.App := vraylib.App(gpu_context[0]); target :: vraylib.RenderTarget := vraylib.RenderTarget(gpu_context[1],gpu_context[0]);
    target.begin(); try { app.clear(vraylib.BLACK); ss_material_apply(gpu_document,m); ss_program(gpu_document).begin(); try { vraylib.draw_rectangle(0,0,8,8,vraylib.WHITE); } finally { ss_program(gpu_document).end(); } } finally { target.end(); }
    return ss_evaluation_capture(gpu_evaluation,target,false);
}
fn ev_gpu_final_failure(m :: MaterialInstance) -> Float64 {
    gpu_rollback_control[1] = gpu_rollback_control[1]+1; loss :: Float64 := ev_gpu_objective(m); if gpu_rollback_control[1] == 3 {
        gpu_evaluation.reference[0] = 99.0; gpu_evaluation.difference[0] = 98.0; gpu_evaluation.metrics[0] = 97.0; gpu_evaluation.gradients[0] = 96.0; gpu_evaluation.optimizer.params[0] = 95.0; gpu_evaluation.state[3] = 94;
        if gpu_rollback_control[0] == 0 { throw "Final GPU candidate failed"; } return float64("nan");
    } return loss;
}
@bounded<67108864> region shader_evaluation_cpu {
    d :: SSDocument := ss_document(); m :: MaterialInstance := ss_material(1,1); ss_material_migrate(m,d); e :: SSEvaluation := ss_evaluation(1,1); ss_evaluation_freeze(e,d,m,1.2345678901234567,ev_camera());
    ss_evaluation_reference(e,[0.2,0.4,0.6,1.0]); loss :: Float64 := ss_evaluation_compare(e,[0.4,0.4,0.3,0.0]); ev_check(vmath.abs(loss-0.13/3.0) < 0.00000001 && vmath.abs(e.metrics[1]-0.5/3.0) < 0.00000001 && e.difference[3] == -1.0,"linear RGB MSE/MAE and signed RGBA difference");
    before :: Float64 := ev_objective(e.material); ss_evaluation_step(e,d,0.001,0.2,ev_objective); ev_check(ev_objective(e.material) < before && ss_material_get(m,1,0) == 0.5,"actual voptim finite-difference update isolated from authored material");
    value :: Float64 := ss_material_get(e.material,1,0); failed :: Bool := false; try { ss_evaluation_step(e,d,0.001,0.2,ev_failure); } catch (error) { failed = true; } ev_check(failed && ss_material_get(e.material,1,0) == value,"failed objective restores perturbation");
    ss_evaluation_save(e,"build/shader-model-dev/checkpoint.vevaluation"); restored :: SSEvaluation := ss_evaluation(1,1); ss_evaluation_load(restored,"build/shader-model-dev/checkpoint.vevaluation"); ev_check(restored.context[0] == e.context[0] && restored.reference[2] == 0.6 && restored.difference[3] == -1.0 && ss_material_get(restored.material,1,0) == value && restored.state[3] == e.state[3],"checkpoint stores exact actual frozen context/reference/parameters/optimizer step");
    ss_evaluation_step(restored,d,0.001,0.2,ev_objective); ss_evaluation_step(e,d,0.001,0.2,ev_objective); ev_check(ss_material_get(e.material,1,0) == ss_material_get(restored.material,1,0),"checkpoint resumes reproducibly");
    ss_parameter_set(d,1,0,0.6); failed = false; value = ss_material_get(e.material,1,0); try { ss_evaluation_step(e,d,0.001,0.2,ev_objective); } catch (error) { failed = true; } ev_check(failed && ss_material_get(e.material,1,0) == value,"changed authored schema/value fingerprint invalidates frozen evaluation");
};
@bounded<67108864> region shader_evaluation_rollback_cpu {
    d :: SSDocument := ss_document(); m :: MaterialInstance := ss_material(1,1); ss_material_migrate(m,d); ss_evaluation_freeze(rollback_evaluation,d,m,4.0,ev_camera()); ss_evaluation_reference(rollback_evaluation,[0.1,0.2,0.3,1.0]); ss_evaluation_compare(rollback_evaluation,[0.2,0.4,0.6,1.0]); before_transaction :: String := ss_evaluation_text(rollback_evaluation); mode :: Int64 := 0;
    while mode < 2 { rollback_control[0] = mode; rollback_control[1] = 0; failed :: Bool := false; try { ss_evaluation_step(rollback_evaluation,d,0.001,0.2,ev_final_failure); } catch (error) { failed = true; } ev_check(failed && rollback_control[1] == 3 && ss_evaluation_text(rollback_evaluation) == before_transaction,"final throw/NaN restores optimizer/material/step/metrics/reference/difference/gradients/context exactly"); mode = mode+1; }
};
@bounded<67108864> region shader_evaluation_roots_cpu {
    m :: MaterialInstance := ss_material(1,1); d :: SSDocument := ss_document(); ss_material_migrate(m,d);
    vfs.write_atomic("build/shader-model-dev/root-child.glsl","vec3 eval_color()\{return vec3(1.0);}\n"); roots_doc :: SSDocument := ss_document(); se_text_write(roots_doc.path,"build/shader-model-dev/root-resource.vshader"); se_text_write(roots_doc.source,"#version 330\n#include \"root-child.glsl\"\nout vec4 finalColor;\nuniform float gain;\nvoid main()\{finalColor=vec4(eval_color()*gain,1.0);}\n"); ss_document_source_change(roots_doc);
    roots :: SSEvaluation := ss_evaluation(1,1); ss_evaluation_freeze(roots,roots_doc,m,3.0,ev_camera()); ss_evaluation_save(roots,"build/shader-model-dev/roots.vevaluation"); roots_reopened :: SSEvaluation := ss_evaluation(1,1); ss_evaluation_load(roots_reopened,"build/shader-model-dev/roots.vevaluation"); restored_roots :: SSDocument := ss_document(); ss_document_parse(restored_roots,se_text_read(roots_reopened.document)); ev_check(restored_roots.path[0] == 0 && restored_roots.fragment_path[0] > 0 && restored_roots.vertex_path[0] > 0 && ss_evaluation_rebind(roots_reopened,restored_roots),"CPU-uncompiled checkpoint serializes effective absolute stage roots despite unsaved document path fallback");
};
out("VMGE_SHADER_EVALUATION_CPU_OK");
if vraylib.environment("VYNE_SHADER_GPU") == "1" {
    app :: vraylib.App := vraylib.app_options(32,32,"Shader evaluation GPU",0,vraylib.FLAG_WINDOW_HIDDEN);
    try {
        d :: SSDocument := gpu_document; ev_check(ss_compile(app,d),"evaluation shader compile"); m :: MaterialInstance := ss_material(1,1); ss_material_migrate(m,d); ss_material_set(m,1,0,0.8); e :: SSEvaluation := gpu_evaluation;
        saved_source :: Array<Int64> := ss_text(65536); se_text_copy(saved_source,d.source); original_evaluation :: String := ss_evaluation_text(e); se_text_write(d.source,"#version 330\nbad source\n"); ss_document_source_change(d); ev_check(!ss_compile(app,d) && ss_program(d).valid(),"old good GPU program survives bad draft"); failed :: Bool := false; try { ss_evaluation_freeze(e,d,m,2.0,ev_camera()); } catch (error) { failed = true; } ev_check(failed && ss_evaluation_text(e) == original_evaluation,"cannot freeze old-good GPU program while current draft compilation failed");
        se_text_copy(d.source,saved_source); ss_document_source_change(d); ev_check(ss_compile(app,d),"restored source compiles for evaluation"); ss_evaluation_freeze(e,d,m,2.0,ev_camera()); target :: vraylib.RenderTarget := app.float_target(8,8); gpu_context[0] = app.token; gpu_context[1] = target.token;
        roots :: SSEvaluation := ss_evaluation(1,1); ss_evaluation_load(roots,"build/shader-model-dev/roots.vevaluation"); roots_doc :: SSDocument := ss_document(); ss_document_parse(roots_doc,se_text_read(roots.document)); ev_check(ss_compile(app,roots_doc) && ss_evaluation_rebind(roots,roots_doc),"reopened checkpoint compiles real relative includes using serialized effective stage roots");
        vfs.write_atomic("build/shader-model-dev/root-child.glsl","vec3 eval_color()\{return vec3(0.5);}\n"); untouched :: SSEvaluation := ss_evaluation(1,1); failed = false; try { ss_evaluation_freeze(untouched,roots_doc,m,3.0,ev_camera()); } catch (error) { failed = true; } ev_check(failed && untouched.state[5] == 0 && ss_document_compile_dirty(roots_doc),"GPU freeze rejects changed includes even without an external watcher poll"); vfs.write_atomic("build/shader-model-dev/root-child.glsl","vec3 eval_color()\{return vec3(1.0);}\n"); ev_check(ss_compile(app,roots_doc) && ss_evaluation_rebind(roots,roots_doc),"restored include resources rebind to current compiled checkpoint");
        app.limit(2); while app.frame() {
            target.begin(); app.clear(vraylib.BLACK); ss_material_apply(d,e.material); ss_program(d).begin(); vraylib.draw_rectangle(0,0,8,8,vraylib.WHITE); ss_program(d).end(); target.end();
            if e.state[2] == 0 { ss_evaluation_capture(e,target,true); } else { ev_check(ss_evaluation_capture(e,target,false) == 0.0,"repeat frozen real GPU capture gives zero loss"); }
            top :: Array<Float64> := target.read_pixel(0,0); ev_check(vmath.abs(top[0]-0.8) < 0.001 && e.reference[0] == top[0],"top-left normalized linear readback");
            if app.frames() == 1 { ss_material_set(e.material,1,0,0.5); before :: Float64 := ev_gpu_objective(e.material); after :: Float64 := ss_evaluation_step(e,d,0.01,0.2,ev_gpu_objective); ev_check(after < before && ss_material_get(m,1,0) == 0.8,"real GPU finite-difference loss improves with voptim while authored reference material stays frozen"); }
            if app.frames() == 1 { before_transaction :: String := ss_evaluation_text(e); mode :: Int64 := 0; while mode < 2 { gpu_rollback_control[0] = mode; gpu_rollback_control[1] = 0; failed = false; try { ss_evaluation_step(e,d,0.01,0.2,ev_gpu_final_failure); } catch (error) { failed = true; } ev_check(failed && gpu_rollback_control[1] == 3 && ss_evaluation_text(e) == before_transaction,"final real GPU objective throw/NaN restores complete evaluation state"); mode = mode+1; } }
            app.begin(); target.present(32.0,32.0); app.end();
        }
        ev_check(e.state[3] == 1,"real GPU finite-difference phase actually executed");
        target.close(); ss_document_close(d); ss_document_close(roots_doc); ev_check(app.resources() == 0,"evaluation GPU releases handles");
    } finally { app.close(); }
    out("VMGE_SHADER_EVALUATION_GPU_OK");
}
out("VMGE_SHADER_EVALUATION_OK");
