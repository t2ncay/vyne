use "../src/editor/shader_workspace.vy";
use native vmem;
fn workspace_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.app_options(1280,800,"Shader workspace checks",0,vraylib.FLAG_WINDOW_HIDDEN);
try {
    root :: String := vfs.join(vraylib.get_working_directory(),"examples/projects/vmge");
    workspace :: SSWorkspace := ss_workspace_create(app,root);
    workspace_check(ss_workspace_status(workspace) == "Compiled","initial compile is not applied");
    workspace_check(ss_workspace_apply(workspace) && ss_workspace_status(workspace) == "Applied" && ss_workspace_take_apply(workspace) && !ss_workspace_take_apply(workspace),"explicit apply edge consumed once");
    good :: Int64 := ss_program(workspace.document).token;
    se_text_write(workspace.fragment.edit_buffer,"bad shader"); workspace.fragment.state[0] = 0; workspace.fragment.state[1] = workspace.document.source[0]; ss_code_insert(workspace.fragment,workspace.fragment.edit_buffer); ss_document_source_change(workspace.document);
    workspace_check(ss_workspace_status(workspace) == "Draft","source edits are Draft");
    workspace_check(!ss_workspace_compile(app,workspace) && ss_workspace_status(workspace) == "Failed" && ss_program(workspace.document).token == good,"failed compile preserves last program and reports failure");
    ss_workspace_reset(workspace); workspace_check(ss_workspace_compile(app,workspace),"reset source compiles");
    original_parameters :: SSParameters := ss_parameters(); ss_parameters_copy(original_parameters,workspace.document.parameters);
    original_material :: MaterialInstance := workspace.material;
    workspace.document.parameters.state[0] = 0; ss_parameter_define(workspace.document,1,"gain",35664,1,0.5,0.0,1.0,"Surface","linear",true,true);
    workspace_check(ss_workspace_parameter_set(workspace,1,1,0.75),"same stable ID can migrate float to vec2 before paired edit");
    vec_slot :: Int64 := ss_material_compatible_slot(workspace.material,1,35664,1); float_slot :: Int64 := ss_material_compatible_slot(workspace.material,1,5126,1);
    workspace_check(vec_slot >= 0 && float_slot >= 0 && workspace.material.meta[vec_slot*5+4] == 1 && workspace.material.meta[float_slot*5+4] == 0 && ss_parameter_get(workspace.document,1,1) == 0.75 && workspace.material.values[vec_slot*64+1] == 0.75,"new shape and document agree while published old shape remains orphaned");
    failed :: Bool := false; edit_revision :: Int64 := workspace.document.state[0]; material_revision :: Int64 := workspace.material.state[3];
    try { ss_workspace_parameter_set(workspace,1,1,1.5); } catch (error) { failed = true; }
    workspace_check(failed && workspace.document.state[0] == edit_revision && workspace.material.state[3] == material_revision && ss_parameter_get(workspace.document,1,1) == 0.75 && workspace.material.values[vec_slot*64+1] == 0.75,"invalid paired edit preserves both values and revisions");
    ss_parameters_copy(workspace.document.parameters,original_parameters);
    small_material :: MaterialInstance := ss_material_sized(1,1,1,0); ss_material_migrate(small_material,workspace.document); workspace.material = small_material;
    workspace.document.parameters.state[0] = 0; ss_parameter_define(workspace.document,1,"gain",35664,1,0.5,0.0,1.0,"Surface","linear",true,true);
    failed = false; edit_revision = workspace.document.state[0]; material_revision = small_material.state[3];
    try { ss_workspace_parameter_set(workspace,1,1,0.75); } catch (error) { failed = true; }
    workspace_check(failed && ss_parameter_get(workspace.document,1,1) == 0.5 && workspace.document.state[0] == edit_revision && small_material.state[3] == material_revision && small_material.state[2] == 1 && small_material.meta[1] == 5126,"failed shape migration capacity cannot diverge document and override");
    workspace_check(!ss_workspace_parameter_edit(workspace,1,1,0.75) && workspace.state[19] == -1 && workspace.log[0] > 0 && ss_parameter_get(workspace.document,1,1) == 0.5 && small_material.state[3] == material_revision,"UI migration failure cancels drag and reports diagnostic without divergence");
    workspace.material = original_material; ss_parameters_copy(workspace.document.parameters,original_parameters); ss_document_source_change(workspace.document); workspace_check(ss_workspace_compile(app,workspace),"restore initial typed schema after review checks");
    se_text_write(workspace.document.path,"build/shader-ui-dev-review-save.vshader"); se_text_write(workspace.layout_path,"build/shader-ui-dev-missing-parent/layout"); workspace.layout.state[6] = 1;
    workspace.graph.meta[3] = 999; # Invalid unrelated graph must not prevent source saving.
    workspace_check(!ss_workspace_save(workspace) && vfs.exists("build/shader-ui-dev-review-save.vshader") && !ss_document_dirty(workspace.document) && workspace.layout.state[6] == 1 && workspace.state[33] == 1,"source saves independently but false layout save propagates and remains pending");
    workspace_check(!ss_workspace_save_layout(workspace) && workspace.layout.state[6] == 1,"retry failure retains dirty layout");
    se_text_write(workspace.layout_path,"build/shader-ui-dev-review-save.layout"); workspace_check(ss_workspace_save_layout(workspace) && workspace.layout.state[6] == 0 && workspace.state[33] == 0,"pending layout retry clears only after successful write");
    workspace.graph.meta[3] = -1;
    se_text_write(workspace.log,"ERROR: 0:12: syntax error\n0(7) : error\n0:4(3): error\n"); ss_workspace_diagnostics(workspace,workspace.log);
    workspace_check(workspace.state[10] == 3 && workspace.diagnostics[0] == 12 && workspace.diagnostics[4] == 7 && workspace.diagnostics[8] == 4 && workspace.diagnostics[9] == 3,"compiler diagnostic forms copied and parsed");
    se_text_write(workspace.log,"vertex:\n0:3(2): error\nfragment:\n0:5(4): error\n"); ss_workspace_diagnostics(workspace,workspace.log);
    workspace_check(ss_workspace_stage_diagnostics(workspace,1) == 1 && workspace.visible_diagnostics[0] == 3,"vertex gutter gets only root vertex locations");
    workspace_check(ss_workspace_stage_diagnostics(workspace,0) == 1 && workspace.visible_diagnostics[0] == 5,"fragment gutter gets only root fragment locations");
    workspace_check(vfs.write_atomic("build/shader-ui-dev-review-include.glsl","vec4 broken = ;\n"),"include diagnostic fixture saved");
    se_text_write(workspace.document.source,"#version 330\n#include \"shader-ui-dev-review-include.glsl\"\nout vec4 finalColor;\nuniform float gain;\nvoid main()\{finalColor=vec4(gain);}\n"); ss_document_source_change(workspace.document);
    workspace_check(!ss_workspace_compile(app,workspace),"actual included shader syntax error fails compile");
    include_row :: Int64 := -1; scan :: Int64 := 0; while scan < workspace.state[10] && include_row < 0 { if workspace.diagnostic_sources[scan] == 1 && workspace.diagnostics[scan*4] > 0 { include_row = scan; } scan = scan+1; }
    workspace_check(include_row >= 0 && workspace.diagnostic_targets[include_row] == 1,"failed build retains include source ID separately from stage and root line");
    root_caret :: Int64 := workspace.fragment.state[0]; opened_path :: Array<Int64> := ss_text(2048);
    workspace_check(ss_workspace_diagnostic_jump(workspace,include_row) && workspace.fragment.state[0] == root_caret && workspace.layout.state[3] == 4 && workspace.panel.state[5] > 0 && !ss_workspace_take_diagnostic_open(workspace,opened_path),"include click opens its own read-only excerpt without a required host event or root jump");
    workspace.panel.state[10] = 1; workspace_check(ss_workspace_take_diagnostic_open(workspace,opened_path) && !ss_workspace_take_diagnostic_open(workspace,opened_path),"explicit external include event is optional and consumed once");
    workspace_check(se_text_read(opened_path) == ss_normal_path(vfs.join(vraylib.get_working_directory(),"build/shader-ui-dev-review-include.glsl")) || se_text_read(opened_path) == "build/shader-ui-dev-review-include.glsl","include event carries exact attempted-build file path");
    workspace_check(workspace.state[48] == 1 && workspace.diagnostic_notice[0] > 0,"include event retains line and explicit log path notice");
    ss_workspace_reset(workspace); workspace_check(ss_workspace_compile(app,workspace),"reset after include failure restores source");
    ss_graph_add(workspace.graph,3,1,4,"review_color",101); ss_graph_value(workspace.graph,3,4,500.0); ss_graph_value(workspace.graph,3,5,500.0);
    workspace_check(ss_graph_generate(workspace.graph,workspace.document),"graph diagnostic source generated through document API"); ss_workspace_sync_source(workspace);
    se_text_write(workspace.log,"fragment:\n3:2(2): node error\n3:1(5): declaration error\n99:8(1): unknown source\n"); ss_workspace_diagnostics(workspace,workspace.log);
    workspace_check(workspace.diagnostic_sources[1] == 3 && workspace.diagnostic_targets[1] == 2 && workspace.diagnostic_nodes[1] == 3,"graph source ID maps to stable node ID");
    workspace_check(workspace.diagnostic_targets[2] == 2 && workspace.diagnostic_nodes[2] == 3,"graph uniform declaration diagnostic maps to exposed node");
    workspace_check(ss_workspace_diagnostic_jump(workspace,1) && workspace.layout.state[1] == 2 && workspace.state[21] == ss_graph_slot(workspace.graph,3) && workspace.values[22] > 0.0,"graph diagnostic selects and pans to node canvas");
    root_caret = workspace.fragment.state[0]; workspace_check(!ss_workspace_diagnostic_jump(workspace,3) && workspace.fragment.state[0] == root_caret && workspace.diagnostic_notice[0] > 0,"unknown source ID remains explicit log notice instead of root jump");
    ss_parameters_copy(workspace.document.parameters,original_parameters); ss_workspace_reset(workspace); workspace_check(ss_workspace_compile(app,workspace),"source restored after graph diagnostic checks");
    workspace.layout.state[1] = 0; workspace.layout.state[6] = 0;
    ss_workspace_diagnostics(workspace,workspace.document.log);
    rgba_source :: vraylib.RenderTarget := app.float_target(4,2); hdr_source :: vraylib.RenderTarget := app.float_target(4,2);
    probe :: vraylib.ShaderBuild := app.compile_shader(null,"#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nvoid main()\{finalColor=vec4(-0.5+fragTexCoord.x,2.0+fragTexCoord.y,0.25,0.375);}\n"); workspace_check(probe.success,"signed/HDR capture fixture compiles");
    workspace_check(app.frame(),"warmup frame opens");
    rgba_source.begin(); app.clear(vraylib.rgba(128,64,32,64)); rgba_source.end();
    source_pixels :: Array<Float64> := rgba_source.read_pixels(); ss_workspace_capture_reference(workspace,rgba_source); reference_target :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[2],app.token); copied_pixels :: Array<Float64> := reference_target.read_pixels(); index :: Int64 := 0;
    while index < source_pixels.size() { workspace_check(vmath.abs(source_pixels[index]-workspace.reference_pixels[index]) < 0.00001 && vmath.abs(source_pixels[index]-copied_pixels[index]) < 0.00001,"reference capture preserves fractional alpha and RGB exactly without blending"); index = index+1; }
    hdr_source.draw_from(rgba_source,probe.shader); hdr_pixels :: Array<Float64> := hdr_source.read_pixels(); ss_workspace_capture_reference(workspace,hdr_source); copied_hdr :: Array<Float64> := reference_target.read_pixels(); index = 0;
    workspace_check(hdr_pixels[0] < 0.0 && hdr_pixels[1] > 1.0 && hdr_pixels[3] == 0.375,"capture fixture contains signed HDR and nonopaque pixels");
    while index < hdr_pixels.size() { workspace_check(vmath.abs(hdr_pixels[index]-workspace.reference_pixels[index]) < 0.00001 && vmath.abs(hdr_pixels[index]-copied_hdr[index]) < 0.00001,"exact reference copy preserves signed HDR channels alpha and row orientation"); index = index+1; }
    workspace.state[12] = 1; workspace.state[14] = 1; workspace.state[35] = 1;
    ss_workspace_preview(app,workspace); workspace_check(workspace.state[26] == 1 && ss_workspace_status(workspace) == "Compiled","capture leaves compiled/applied distinction intact");
    workspace.state[13] = 2; workspace.state[27] = 1; ss_workspace_preview(app,workspace);
    workspace_check(workspace.values[3] < 0.001,"identical frozen capture has zero pixel difference");
    ss_workspace_parameter_set(workspace,1,0,0.75); workspace.state[27] = 1; ss_workspace_preview(app,workspace);
    workspace_check(workspace.values[3] > 0.2,"changed typed parameter produces actual pixel difference");
    difference :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[3],app.token); visual_difference :: Array<Float64> := difference.read_pixels();
    workspace_check(visual_difference[(128*256+128)*4] > 0.7,"difference preview shader shows amplified actual pixel error");
    ss_workspace_parameter_set(workspace,1,0,0.5);
    workspace.state[24] = 1; ss_workspace_preview(app,workspace); workspace_check(workspace.pipeline.outputs[0] != 0,"pipeline produces inspectable output");
    workspace.state[15] = 128; ss_workspace_preview(app,workspace); workspace_check(workspace.state[26] == 0,"resolution change invalidates reference");
    workspace.state[24] = 0; workspace.state[13] = 0; workspace.layout.values[3] = 1.0;
    app.begin(); app.clear(vraylib.BLACK); app.end(); app.end_frame();
    bounds :: vraylib.Rectangle := se_rect(0.0,0.0,1280.0,800.0); warm :: Int64 := vmem.commit_total_allocated(); app.limit(7);
    while app.frame() {
        workspace.layout.state[1] = app.frames()%4;
        if app.frames() >= 5 { workspace.layout.values[3] = 1.5; }
        ss_workspace_frame(app,workspace,bounds,1.0/60.0); app.begin(); app.clear(vraylib.BLACK); ss_workspace_navigation(workspace.layout,bounds); ss_workspace_draw(app,workspace,bounds);
        if workspace.layout.state[1] == 0 { vraylib.gl_draw_render_batch_active(); vraylib.take_screenshot("build/shader-ui-dev-workspace.png"); }
        if workspace.layout.state[1] == 2 { vraylib.gl_draw_render_batch_active(); vraylib.take_screenshot("build/shader-ui-dev-graph.png"); }
        app.end();
        workspace_check(app.passes() == 0,"workspace passes balance for all tabs");
        workspace_check(vmem.commit_total_allocated() == warm,"workspace retains no frame-created boxes");
    }
} finally { app.close(); }
workspace_check(vraylib.live_handles() == 0,"workspace resources close with shared app");
out("VMGE_SHADER_UI_WORKSPACE_OK");
