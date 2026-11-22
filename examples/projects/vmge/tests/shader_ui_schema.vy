use "../src/editor/shader_workspace.vy";
use native vmem;
fn schema_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.app_options(1280,800,"Shader authoring workflow",0,vraylib.FLAG_WINDOW_HIDDEN);
try {
    workspace :: SSWorkspace := ss_workspace_create(app,vfs.join(vraylib.get_working_directory(),"examples/projects/vmge"));
    se_text_write(workspace.layout_path,"build/shader-ui-dev-schema.layout");
    schema_check(workspace.panel.state[20] <= 131072,"new retained panel owners fit the 128 KiB host allowance");
    out("VMGE_SHADER_UI_PANEL allocation_bytes="+string(workspace.panel.state[20]));
    se_text_write(workspace.document.source,"#version 330\nout vec4 finalColor;\nuniform vec3 tint;\nvoid main()\{finalColor=vec4(tint,1.0);}\n"); ss_document_source_change(workspace.document);
    schema_check(ss_workspace_compile(app,workspace),"new GLSL uniform compiles without an encoded resource schema");
    row :: Int64 := -1; i :: Int64 := 0; while i < workspace.panel.state[0] { if ss_panel_name(workspace.panel,i) == "tint" { row = i; } i = i+1; }
    schema_check(row >= 0 && workspace.panel.reflection[row*4] == 35665 && workspace.panel.reflection[row*4+1] == 1,"unrepresented vec3 is discoverable in Expose UI");
    schema_check(ss_uniform_builtin("mvp") && ss_uniform_builtin("texture0") && !ss_uniform_builtin("tint"),"builtins are filtered from exposure");
    schema_check(ss_uniform_support("block.member",35665,1,-1) != 0 && ss_uniform_support("tex",35678,2,0) == 4 && ss_uniform_support("unknown",5125,1,0) == 3,"negative/block/sampler-array/unknown uniforms have explicit unsupported status");
    schema_check(ss_workspace_expose_begin(workspace,row),"Expose opens a retained metadata draft"); id :: Int64 := workspace.panel.draft[1];
    before_doc :: String := ss_document_text(workspace.document); before_material :: String := ss_material_text(workspace.material); program :: Int64 := ss_program(workspace.document).token;
    workspace.panel.numbers[4] = 2.0;
    schema_check(!ss_workspace_schema_apply(app,workspace) && ss_document_text(workspace.document) == before_doc && ss_material_text(workspace.material) == before_material && ss_program(workspace.document).token == program,"invalid component metadata is atomic and leaves publication unchanged");
    se_edit_begin(workspace.parameter_editor,200,id,"0.25"); schema_check(ss_workspace_commit_field(workspace),"default field dispatch");
    se_edit_begin(workspace.parameter_editor,201,id,"0.5"); ss_workspace_commit_field(workspace);
    se_edit_begin(workspace.parameter_editor,202,id,"0.75"); ss_workspace_commit_field(workspace);
    se_edit_begin(workspace.parameter_editor,102,id,"Surface Color"); ss_workspace_commit_field(workspace);
    se_edit_begin(workspace.parameter_editor,103,id,"linear RGB"); ss_workspace_commit_field(workspace);
    workspace.panel.draft[6] = 1;
    schema_check(ss_workspace_schema_apply(app,workspace),"validated schema applies and recompiles for immediate editing");
    slot :: Int64 := ss_parameter_slot(workspace.document.parameters,id);
    schema_check(slot >= 0 && workspace.document.parameters.meta[slot*8+3] == 3 && workspace.document.parameters.meta[slot*8+6] == 1 && se_text_read(workspace.document.parameters.groups[slot]) == "Surface Color" && se_text_read(workspace.document.parameters.units[slot]) == "linear RGB","stable typed metadata reaches active resource schema");
    schema_check(ss_workspace_schema_begin(workspace,id),"existing metadata editable");
    before_doc = ss_document_text(workspace.document); before_material = ss_material_text(workspace.material); workspace.panel.numbers[1] = 0.1;
    schema_check(!ss_workspace_schema_apply(app,workspace) && ss_document_text(workspace.document) == before_doc && ss_material_text(workspace.material) == before_material,"new range validates every current/default/override component before publication");
    workspace.panel.numbers[1] = 1.0; workspace.panel.draft[5] = 0; workspace.panel.draft[6] = 1;
    schema_check(!ss_workspace_schema_apply(app,workspace) && ss_document_text(workspace.document) == before_doc,"invalid continuous/fit flags are atomic"); workspace.panel.draft[0] = 0;
    opened :: Array<Int64> := ss_text(2048);
    schema_check(vfs.write_atomic("build/shader-ui-dev-excerpt.glsl","// first\n// second\nvec3 included_tint = vec3(0.2,0.4,0.6);\n"),"include excerpt fixture parent exists");
    schema_check(app.frame(),"warmup frame"); workspace.state[35] = 1; workspace.state[14] = 1;
    ss_workspace_preview(app,workspace); target :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[1],app.token); pixel :: Array<Float64> := target.read_pixel(128,128);
    schema_check(vmath.abs(pixel[0]-0.25) < 0.001 && vmath.abs(pixel[1]-0.5) < 0.001 && vmath.abs(pixel[2]-0.75) < 0.001,"new source uniform -> expose -> defaults produces real RGB pixels");
    ss_value_begin(workspace.panel,workspace.document,workspace.material,id); workspace.state[19] = slot; ss_workspace_parameter_edit(workspace,id,0,0.6); ss_workspace_parameter_edit(workspace,id,0,0.8); workspace.state[19] = -1; ss_value_end(workspace.panel,workspace.document,workspace.material);
    schema_check(workspace.panel.history_state[6] == 1,"one drag stroke creates one selective transaction");
    schema_check(ss_workspace_parameter_undo(workspace,false) && ss_parameter_get(workspace.document,id,0) == 0.25,"value stroke undo"); schema_check(ss_workspace_parameter_undo(workspace,true) && ss_parameter_get(workspace.document,id,0) == 0.8,"value stroke redo");
    ss_workspace_preview(app,workspace); pixel = target.read_pixel(128,128); schema_check(vmath.abs(pixel[0]-0.8) < 0.001,"edited exposed value updates pixels");
    schema_check(ss_workspace_parameter_reset(workspace,id) && workspace.panel.history_state[6] == 2 && ss_parameter_get(workspace.document,id,0) == 0.25,"reset is a single undoable transaction");
    schema_check(ss_workspace_parameter_undo(workspace,false) && ss_parameter_get(workspace.document,id,0) == 0.8,"reset undo restores prior values"); ss_workspace_parameter_undo(workspace,true);
    precise :: Float64 := 0.1234567890123; ss_workspace_parameter_set(workspace,id,0,precise); schema_check(ss_workspace_value_field_begin(workspace,id,0) && ss_workspace_commit_field(workspace) && ss_parameter_get(workspace.document,id,0) == precise,"unchanged exact field preserves full Float64 despite rounded display formatting"); ss_workspace_parameter_set(workspace,id,0,0.25);
    old_path :: String := se_text_read(workspace.document.path);
    schema_check(!ss_workspace_save_as_request(workspace,"build/shader-ui-dev-absent-save-dir/source.vshader",true) && se_text_read(workspace.document.path) == old_path,"failed Save As retains source identity"); workspace.panel.state[2] = 0;
    schema_check(ss_workspace_save_as_request(workspace,"build/shader-ui-dev-authoring.vshader",true),"source Save As succeeds atomically");
    ss_workspace_parameter_set(workspace,id,0,0.9); schema_check(!ss_workspace_reload(workspace,false) && ss_parameter_get(workspace.document,id,0) == 0.9 && workspace.document.state[6] == 1,"Reload requires explicit discard for local conflict");
    schema_check(ss_workspace_reload(workspace,true) && ss_parameter_get(workspace.document,id,0) == 0.25 && !ss_document_dirty(workspace.document),"explicit Reload restores disk schema and values");
    workspace.state[42] = 1; old_path = se_text_read(workspace.document.path);
    schema_check(ss_workspace_save_as_request(workspace,"build/shader-ui-dev-host-only-target.vshader",true) && workspace.state[47] == 1 && se_text_read(workspace.document.path) == old_path,"hosted Save As queues checkpoint hook without changing identity");
    schema_check(!ss_workspace_save_as_complete(workspace,false) && workspace.state[47] == 0 && workspace.parameter_editor.state[1] == 900 && se_text_read(workspace.document.path) == old_path,"host Save As failure reopens path field and retains identity");
    workspace.panel.state[2] = 0; workspace.parameter_editor.state[1] = -1;
    schema_check(ss_workspace_reload(workspace,true) && workspace.state[53] == 1 && se_text_read(workspace.document.path) == old_path,"hosted Reload is a separate checkpoint hook"); ss_workspace_reload_complete(workspace,true); workspace.state[42] = 0;
    app.begin(); app.clear(vraylib.BLACK); schema_clip :: vraylib.Clip := app.clip(se_rect(1000.0,80.0,280.0,500.0)); se_clip_start(schema_clip,se_rect(1000.0,80.0,280.0,500.0));
    try { schema_check(ss_workspace_schema_begin(workspace,id) && ss_workspace_schema_apply(app,workspace),"Apply Schema works from an active inspector drawing clip"); } finally { se_clip_finish(schema_clip); }
    schema_check(ss_workspace_include_view(workspace,"build/shader-ui-dev-excerpt.glsl",3,1) && workspace.layout.state[3] == 4 && workspace.panel.state[5] > 0,"include drawer has an actual read-only excerpt");
    schema_check(se_edit_slice(workspace.messages,2*513,2*513+workspace.messages[2*513]) == "> 3  vec3 included_tint = vec3(0.2,0.4,0.6);","excerpt contains requested source line and marker");
    schema_check(!ss_workspace_take_diagnostic_open(workspace,opened),"include viewer does not require an unresolved host-open event");
    reads :: Int64 := workspace.panel.state[7]; schema_check(vfs.write_atomic("build/shader-ui-dev-excerpt.glsl","// changed externally\n"),"external fixture change");
    app.end(); app.end_frame();
    warm :: Int64 := vmem.commit_total_allocated(); app.limit(5); bounds :: vraylib.Rectangle := se_rect(0.0,0.0,1280.0,800.0);
    workspace.layout.state[2] = 0;
    while app.frame() {
        if app.frames() == 1 { ss_workspace_schema_begin(workspace,id); }
        if app.frames() == 2 { se_edit_begin(workspace.parameter_editor,102,id,"Draft survives poisoned frames"); }
        if app.frames() == 3 { schema_check(ss_workspace_commit_field(workspace) && se_text_read(workspace.panel.group_name) == "Draft survives poisoned frames","metadata field uses retained draft buffers across frames"); }
        if app.frames() == 4 { workspace.panel.draft[0] = 0; ss_workspace_save_as_begin(workspace); }
        ss_workspace_frame(app,workspace,bounds,1.0/60.0); app.begin(); app.clear(vraylib.BLACK); ss_workspace_navigation(workspace.layout,bounds); ss_workspace_draw(app,workspace,bounds);
        if app.frames() == 1 { vraylib.gl_draw_render_batch_active(); vraylib.take_screenshot("build/shader-ui-dev-schema-form.png"); }
        if app.frames() == 4 { vraylib.gl_draw_render_batch_active(); vraylib.take_screenshot("build/shader-ui-dev-save-as.png"); }
        app.end();
        schema_check(workspace.panel.state[7] == reads && workspace.panel.state[5] > 0,"read-only context is cached, not reread each frame");
        schema_check(app.passes() == 0 && vmem.commit_total_allocated() == warm,"authoring forms/viewer retain no frame-created owners and balance passes");
    }
} finally { app.close(); }
schema_check(vraylib.live_handles() == 0,"shared session owns all authoring resources"); out("VMGE_SHADER_UI_SCHEMA_OK");
