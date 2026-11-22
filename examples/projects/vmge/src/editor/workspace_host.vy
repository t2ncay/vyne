use "shader_workspace.vy";
use "workspace.vy";
use "document.vy";

# state: 0 modal (1 close, 2 open), 1/2 saved graph/pipeline revisions, 3 reserved,
# 4 source tab restore, 5 close approval, 6 pending selected node, 7 sampler index,
# 8 semantic material revision, 9 checkpoint matches editable disk, 10 save failure,
# 11 reserved, 12 source disk revision, 13 resolve bindings, 14 cached frame capture.
# All saved/staged text and schema owners are acquired before the first frame.
interface SEWorkspaceHost {
    workspace :: SSWorkspace, state :: Array<Int64>, pending :: Array<Int64>, publication :: SSDocument,
    resources :: Array, resource_state :: Array<Int64>, publish_directory :: Array<Int64>, editable_path :: Array<Int64>,
    saved_material :: MaterialInstance, loaded_material :: MaterialInstance, saved_parameters :: SSParameters,
    saved_resources :: Array, loaded_resources :: Array, saved_resource_state :: Array<Int64>, loaded_resource_state :: Array<Int64>,
    graph_path :: Array<Int64>, pipeline_path :: Array<Int64>,
    render_state :: Array<Int64>,
}
fn se_workspace_host(app :: vraylib.App,project :: String) -> SEWorkspaceHost {
    w :: SSWorkspace := ss_workspace_create(app,project); w.layout.state[0] = 0;
    resources :: Array := []; resources.reserve(64); i :: Int64 := 0; while i < 64 { resources.push(ss_text(255)); i = i+1; }
    directory :: Array<Int64> := ss_text(2048); se_text_write(directory,"shaders/studio");
    state :: Array<Int64> := ss_ints(16); state[1] = w.graph.state[1]; state[2] = w.pipeline.state[1]; state[6] = -1;
    editable :: Array<Int64> := ss_text(2048); se_text_copy(editable,w.document.path);
    saved_resources :: Array := []; loaded_resources :: Array := []; saved_resources.reserve(64); loaded_resources.reserve(64);
    i = 0; while i < 64 { saved_resources.push(ss_text(255)); loaded_resources.push(ss_text(255)); i = i+1; }
    graph_path :: Array<Int64> := ss_text(2048); pipeline_path :: Array<Int64> := ss_text(2048);
    se_text_write(graph_path,vmge_option("VYNE_VMGE_SHADER_GRAPH",vfs.join(project,"shaders/studio.vgraph"))); se_text_write(pipeline_path,vmge_option("VYNE_VMGE_SHADER_PIPELINE",vfs.join(project,"shaders/studio.vpipeline")));
    host :: SEWorkspaceHost := SEWorkspaceHost(w,state,ss_text(2048),ss_document(),resources,ss_ints(1),directory,editable,ss_material(1,1),ss_material(1,1),ss_parameters(),saved_resources,loaded_resources,ss_ints(1),ss_ints(1),graph_path,pipeline_path,ss_ints(16));
    host.render_state[4] = 2;
    w.state[42] = 1; se_host_checkpoint(host);
    manifest :: String := se_host_material_path(se_text_read(editable));
    if vfs.exists(manifest) { se_text_copy(host.pending,editable); host.state[0] = 2; se_host_finish(app,host); }
    return host;
}
fn se_host_material_path(path :: String) -> String { return vfs.join(vfs.dirname(path),vfs.stem(path)+".vmaterial"); }
fn se_host_bind_registry(host :: SEWorkspaceHost,bank :: SEAssets) -> Bool {
    bank.programs.documents[8] = host.workspace.document; bank.programs.documents[9] = host.workspace.pass_document;
    ids :: Array<Int64> := ss_ints(10); i :: Int64 := 0; while i < 8 { ids[i] = i+3; i = i+1; } ids[8] = 1; ids[9] = 2;
    host.workspace.documents = bank.programs.documents; host.workspace.document_ids = ids;
    return true;
}
fn se_host_checkpoint(host :: SEWorkspaceHost) -> Bool {
    w :: SSWorkspace := host.workspace; se_material_copy(host.saved_material,w.material); ss_parameters_copy(host.saved_parameters,w.document.parameters);
    host.saved_resource_state[0] = host.resource_state[0]; i :: Int64 := 0;
    while i < 64 { se_text_copy(host.saved_resources[i],host.resources[i]); i = i+1; }
    host.state[8] = w.material.state[3]; host.state[9] = int64(!ss_document_dirty(w.document)); host.state[12] = w.document.state[1]; return true;
}
fn se_host_flush_layout(host :: SEWorkspaceHost) -> Bool {
    if !ss_workspace_save_layout(host.workspace) { host.state[10] = 1; host.workspace.layout.state[6] = 1; return false; }
    return true;
}
fn se_host_material_text(host :: SEWorkspaceHost) -> String { return se_host_material_text_for(host,host.workspace.document); }
fn se_host_material_text_for(host :: SEWorkspaceHost,d :: SSDocument) -> String {
    shader :: Array<Int64> := ss_text(600000); se_text_write(shader,ss_document_text(d));
    b :: Int64 := vcore.sb_create(); vcore.sb_append(b,"VMGE_STUDIO_MATERIAL|1\nS|"+se_text_hex(shader)+"\n");
    vcore.sb_append(b,ss_material_text(host.workspace.material)); i :: Int64 := 0;
    while i < host.resource_state[0] { vcore.sb_append(b,"R|"+string(i+100)+"|"+se_text_hex(host.resources[i])+"\n"); i = i+1; }
    return vcore.sb_build(b);
}
fn se_host_restore_file(path :: String,existed :: Bool,text :: String) -> Bool {
    if existed { return vfs.write_atomic(path,text); }
    if vfs.exists(path) { return vfs.remove(path); } return true;
}
fn se_host_save_as(host :: SEWorkspaceHost,destination :: String) -> Bool {
    w :: SSWorkspace := host.workspace; success :: Bool := false;
    @bounded<67108864> region host_save_as {
        path :: String := destination; if !vraylib.is_path_absolute(path) { path = vfs.join(vraylib.get_working_directory(),path); }
        sidecar :: String := se_host_material_path(path); old_source :: String := ""; old_material :: String := "";
        source_existed :: Bool := vfs.exists(path); material_existed :: Bool := vfs.exists(sidecar);
        source_written :: Bool := false; material_written :: Bool := false;
        try {
            se_text_write(host.pending,path);
            if path.size() > 2048 || vfs.extension(path) != ".vshader" || se_material_publication(path) { throw "Save As requires a mutable .vshader path of at most 2048 bytes"; }
            if !vfs.is_dir(vfs.dirname(path)) { throw "Save As destination directory does not exist"; }
            if source_existed { old_source = vfs.read(path); if old_source == null { throw "Cannot preserve existing Save As source"; } }
            if material_existed { old_material = vfs.read(sidecar); if old_material == null { throw "Cannot preserve existing Save As material"; } }
            d :: SSDocument := host.publication;
            se_text_copy(d.source,w.document.source); se_text_copy(d.vertex,w.document.vertex); ss_parameters_copy(d.parameters,w.document.parameters);
            se_text_copy(d.fragment_path,w.document.fragment_path); se_text_copy(d.vertex_path,w.document.vertex_path);
            if d.fragment_path[0] == 0 { se_text_copy(d.fragment_path,w.document.path); }
            if d.vertex_path[0] == 0 && d.vertex[0] != 0 { se_text_copy(d.vertex_path,w.document.path); }
            if !se_host_flush_layout(host) { throw "Save As layout write failed; previous identity retained"; }
            if !ss_document_save(d,path) { throw "Save As source write failed"; } source_written = true;
            if !vfs.write_atomic(sidecar,se_host_material_text_for(host,d)) { throw "Save As material checkpoint write failed"; } material_written = true;
            se_text_copy(w.document.path,d.path); se_text_copy(w.document.fragment_path,d.fragment_path); se_text_copy(w.document.vertex_path,d.vertex_path);
            se_text_copy(host.editable_path,d.path); w.document.state[7] = d.state[7]; w.document.state[1] = w.document.state[0]; w.document.state[6] = 0;
            se_text_copy(w.saved_source,w.document.source); se_text_copy(w.saved_vertex,w.document.vertex); se_host_checkpoint(host);
            host.state[10] = 0; w.state[33] = 0; success = true;
        } catch (error) {
            restored :: Bool := true;
            if material_written { if !se_host_restore_file(sidecar,material_existed,old_material) { restored = false; } }
            if source_written { if !se_host_restore_file(path,source_existed,old_source) { restored = false; } }
            host.state[10] = 1; w.state[33] = 1; message :: String := string(error); if !restored { message = message+" / destination rollback failed"; }
            se_text_write(w.log,message); ss_workspace_diagnostics(w,w.log);
        }
    };
    ss_workspace_save_as_complete(w,success); if success { host.pending[0] = 0; } return success;
}
fn se_host_reload(app :: vraylib.App,host :: SEWorkspaceHost,bank :: SEAssets) -> Bool {
    w :: SSWorkspace := host.workspace;
    if host.state[0] != 0 { se_text_write(w.log,"Finish the pending host action before Reload"); ss_workspace_reload_complete(w,false); return false; }
    if w.panel.state[3] == 0 {
        if ss_document_dirty(w.document) || se_host_material_dirty(host) || w.panel.draft[0] != 0 || w.parameter_editor.state[1] >= 0 { ss_workspace_reload_complete(w,false); return false; }
    }
    success :: Bool := false;
    try {
        se_text_copy(host.pending,w.document.path); host.state[0] = 2; se_host_finish(app,host); se_host_resolve_editable(host,bank); success = true;
    } catch (error) { host.state[0] = 0; host.pending[0] = 0; se_text_write(w.log,string(error)); ss_workspace_diagnostics(w,w.log); }
    ss_workspace_reload_complete(w,success); return success;
}
fn se_host_load_material(host :: SEWorkspaceHost,text :: String) -> Bool {
    if text.size() > 2097152 { throw "Studio material checkpoint exceeds 2 MiB"; }
    rows :: Array := ss_split(text,"\n"); if rows[0] != "VMGE_STUDIO_MATERIAL|1" { throw "Unsupported studio material checkpoint"; }
    material :: Int64 := vcore.sb_create(); shader :: Array<Int64> := ss_text(600000); seen :: Bool := false;
    host.loaded_resource_state[0] = 0; i :: Int64 := 1;
    while i < rows.size() { row :: String := rows[i]; if row != "" {
        if row[0] == "S" { if seen { throw "Duplicate studio shader checkpoint"; } fields :: Array := ss_split(row,"|"); if fields.size() != 2 { throw "Invalid studio shader checkpoint"; } ss_unhex(shader,fields[1]); seen = true; }
        else { if row[0] == "R" {
            fields :: Array := ss_split(row,"|"); count :: Int64 := host.loaded_resource_state[0];
            if fields.size() != 3 || count >= 64 || ss_integer(fields[1]) != count+100 { throw "Invalid studio texture resource table"; }
            ss_unhex(host.loaded_resources[count],fields[2]); host.loaded_resource_state[0] = count+1;
        } else { vcore.sb_append(material,row+"\n"); } }
    } i = i+1; }
    if !seen { throw "Missing shader in studio material checkpoint"; }
    ss_document_parse(host.publication,se_text_read(shader)); ss_material_parse(host.loaded_material,vcore.sb_build(material));
    i = 0; while i < host.loaded_material.binding_state[0] { id :: Int64 := host.loaded_material.bindings[i*4+1]-100; if id < 0 || id >= host.loaded_resource_state[0] { throw "Missing studio sampler resource reference"; } i = i+1; }
    return true;
}
fn se_host_save_editable(host :: SEWorkspaceHost) -> Bool {
    w :: SSWorkspace := host.workspace; disk_revision :: Int64 := host.state[12]; success :: Bool := false;
    @bounded<67108864> region host_editable_save { try {
        se_host_commit_parameter(host);
        path :: String := se_text_read(w.document.path); se_material_guard_source(w.document,path);
        if !se_host_flush_layout(host) { throw "Cannot save studio layout; pending action retained"; }
        ss_document_save(w.document,path);
        # The single atomic material checkpoint includes the full shader, values,
        # sampler IDs and R paths. It is the authoritative completed Save commit.
        if !vfs.write_atomic(se_host_material_path(path),se_host_material_text(host)) { throw "Cannot atomically save studio material checkpoint"; }
        se_text_copy(w.saved_source,w.document.source); se_text_copy(w.saved_vertex,w.document.vertex);
        se_host_checkpoint(host); host.state[10] = 0; w.state[33] = 0; success = true;
    } catch (error) {
        w.document.state[1] = disk_revision; host.state[10] = 1; w.state[33] = 1;
        se_text_write(w.log,string(error)); ss_workspace_diagnostics(w,w.log);
    } }; return success;
}
fn se_host_commit_parameter(host :: SEWorkspaceHost) -> Bool {
    w :: SSWorkspace := host.workspace;
    if w.parameter_editor.state[1] < 0 { return true; }
    if !ss_workspace_commit_field(w) { throw "Pending authoring field was not committed: "+se_text_read(w.log); }
    ss_workspace_claim_input(w); return true;
}
fn se_host_reset(host :: SEWorkspaceHost) -> Bool {
    w :: SSWorkspace := host.workspace;
    se_text_copy(w.document.source,w.saved_source); se_text_copy(w.document.vertex,w.saved_vertex);
    ss_parameters_copy(w.document.parameters,host.saved_parameters); ss_document_source_change(w.document); w.document.state[8] = w.document.state[8]+1;
    se_material_copy(w.material,host.saved_material); host.resource_state[0] = host.saved_resource_state[0];
    i :: Int64 := 0; while i < 64 { se_text_copy(host.resources[i],host.saved_resources[i]); i = i+1; }
    w.document.state[1] = 0; if host.state[9] == 1 { w.document.state[1] = w.document.state[0]; }
    host.state[12] = w.document.state[1]; host.state[8] = w.material.state[3]; host.state[10] = 0;
    w.state[9] = 0; host.state[13] = 1; ss_workspace_sync_source(w); ss_workspace_reset_complete(w); return true;
}
fn se_host_resolve_editable(host :: SEWorkspaceHost,bank :: SEAssets) -> Bool {
    m :: MaterialInstance := host.workspace.material; i :: Int64 := 0;
    while i < m.binding_state[0] { source :: Int64 := m.bindings[i*4+1]-100; if source < 0 || source >= host.resource_state[0] { throw "Unknown editable sampler reference"; }
        path :: String := se_text_read(host.resources[source]); ss_material_bind_texture(m,m.bindings[i*4],source+100,se_material_texture(bank,path)); i = i+1;
    } host.state[13] = 0; return true;
}
fn se_host_actions(app :: vraylib.App,host :: SEWorkspaceHost,bank :: SEAssets) -> Bool {
    w :: SSWorkspace := host.workspace;
    if w.state[47] != 0 { return se_host_save_as(host,ss_workspace_save_as_path(w)); }
    if w.state[53] != 0 { return se_host_reload(app,host,bank); }
    if w.state[46] != 0 { return se_host_reset(host); }
    if w.state[43] != 0 { w.state[43] = 0; return se_host_save_editable(host); }
    if ss_workspace_take_diagnostic_open(w,host.pending) {
        path :: String := se_text_read(host.pending);
        se_text_write(w.diagnostic_notice,"External open requested: "+path+". Read-only include context remains available; no OS launcher is configured."); host.pending[0] = 0;
    }
    return true;
}
fn se_host_dirty(host :: SEWorkspaceHost) -> Bool {
    w :: SSWorkspace := host.workspace;
    @bounded<16777216> region host_saved_revisions {
        root :: String := se_text_read(w.project);
        if host.state[1] != w.graph.state[1] {
            path :: String := se_text_read(host.graph_path);
            if vfs.exists(path) { raw := vfs.read(path); if raw != null && raw == ss_graph_text(w.graph) { host.state[1] = w.graph.state[1]; } }
        }
        if host.state[2] != w.pipeline.state[1] {
            path :: String := se_text_read(host.pipeline_path);
            if vfs.exists(path) { raw := vfs.read(path); if raw != null && raw == ss_pipeline_text(w.pipeline) { host.state[2] = w.pipeline.state[1]; } }
        }
    };
    return ss_document_dirty(w.document) || se_host_material_dirty(host) || w.parameter_editor.state[1] >= 0 || w.panel.draft[0] != 0 || host.state[10] != 0 || w.layout.state[6] != 0 || host.state[1] != w.graph.state[1] || host.state[2] != w.pipeline.state[1];
}
fn se_host_material_dirty(host :: SEWorkspaceHost) -> Bool {
    m :: MaterialInstance := host.workspace.material; dirty :: Bool := false;
    if host.state[8] != m.state[3] { dirty = ss_material_text(m) != ss_material_text(host.saved_material); if !dirty { host.state[8] = m.state[3]; } }
    if host.resource_state[0] != host.saved_resource_state[0] { return true; }
    i :: Int64 := 0; while i < host.resource_state[0] { if se_text_read(host.resources[i]) != se_text_read(host.saved_resources[i]) { dirty = true; } i = i+1; }
    return dirty;
}
fn se_host_save(host :: SEWorkspaceHost) -> Bool {
    w :: SSWorkspace := host.workspace;
    if !se_host_save_editable(host) { return false; }
    try {
        if !ss_graph_save(w.graph,se_text_read(host.graph_path)) { throw "Graph save failed"; } host.state[1] = w.graph.state[1];
        if !se_host_pipeline_save_root(host,se_text_read(w.project)) { throw "Pipeline and pass-resource ledger save failed"; } host.state[2] = w.pipeline.state[1];
        host.state[10] = 0; return true;
    } catch (error) { host.state[10] = 1; w.state[33] = 1; se_text_write(w.log,string(error)); ss_workspace_diagnostics(w,w.log); return false; }
}
fn se_host_navigation_ready(host :: SEWorkspaceHost,ui :: SEUI,active :: Int64) -> Bool {
    if host.state[0] != 0 || host.state[14] != 0 || ss_workspace_input_captured(host.workspace) || host.workspace.layout.state[5] != 0 { return false; }
    if active < 2 && (ui.state[39] != 0 || ui.state[43] != 0) { return false; }
    return true;
}
fn se_host_open(app :: vraylib.App,host :: SEWorkspaceHost,path :: String) -> Bool {
    if path == "" { host.workspace.layout.state[0] = 2; return true; }
    se_text_write(host.pending,path); host.state[0] = 2;
    if se_host_dirty(host) { return false; }
    return se_host_finish(app,host);
}
fn se_host_finish(app :: vraylib.App,host :: SEWorkspaceHost) -> Bool {
    if host.state[0] != 2 { return false; }
    w :: SSWorkspace := host.workspace; source_path :: String := se_text_read(host.pending); manifest :: String := se_host_material_path(source_path);
    bundled :: Bool := false;
    @bounded<67108864> region host_document_replacement {
        se_material_reset(host.loaded_material,1,1); host.loaded_resource_state[0] = 0;
        if !se_material_publication(source_path) { if vfs.exists(manifest) { raw := vfs.read(manifest); if raw == null { throw "Cannot read studio material checkpoint"; } se_host_load_material(host,raw); bundled = true; } }
        if !bundled { ss_document_load(host.publication,source_path); }
        staged :: SSDocument := host.publication;
        if staged.fragment_path[0] == 0 { se_text_write(staged.fragment_path,source_path); }
        if staged.vertex_path[0] == 0 { se_text_write(staged.vertex_path,source_path); }
        se_text_copy(w.document.source,staged.source); se_text_copy(w.document.vertex,staged.vertex); ss_parameters_copy(w.document.parameters,staged.parameters);
        se_text_copy(w.document.fragment_path,staged.fragment_path); se_text_copy(w.document.vertex_path,staged.vertex_path);
        se_text_copy(w.document.path,host.editable_path); ss_document_source_change(w.document); w.document.state[8] = w.document.state[8]+1;
        w.document.state[1] = 0;
        if bundled && source_path == se_text_read(host.editable_path) { w.document.state[1] = w.document.state[0]; }
    };
    se_material_reset(w.material,1,1); host.resource_state[0] = host.loaded_resource_state[0];
    i :: Int64 := 0; while i < 64 { se_text_copy(host.resources[i],host.loaded_resources[i]); i = i+1; }
    ss_workspace_sync_source(w); se_text_copy(w.saved_source,w.document.source); se_text_copy(w.saved_vertex,w.document.vertex);
    ss_workspace_compile(app,w); w.layout.state[0] = 2; w.layout.state[1] = 0;
    if bundled { se_material_copy(w.material,host.loaded_material); }
    w.parameter_editor.state[1] = -1; w.panel.draft[0] = 0; w.panel.state[2] = 0; w.state[55] = 1;
    ss_code_reset_history(w.fragment); ss_code_reset_history(w.vertex); ss_value_history_clear(w.panel);
    w.state[17] = 0; host.state[7] = 0; host.state[10] = 0; host.state[13] = 1; se_host_checkpoint(host);
    host.state[0] = 0; host.pending[0] = 0; return true;
}
fn se_host_apply(app :: vraylib.App,host :: SEWorkspaceHost,scene :: SEScene,history :: SEHistory,assets :: SEAssets,id :: Int64) -> Bool {
    w :: SSWorkspace := host.workspace;
    if !scene.valid(id) { throw "Apply requires a selected mesh in the Scene inspector"; }
    if scene.kind(id) != 1 { throw "Apply requires a selected mesh in the Scene inspector"; }
    ss_document_dependencies_changed(w.document);
    if ss_document_compile_dirty(w.document) || !ss_program(w.document).valid() { throw "Compile the current shader and include dependencies before Apply"; }
    se_material_limit(w.material);
    @bounded<67108864> region host_publish {
        d :: SSDocument := host.publication;
        se_text_copy(d.source,w.document.source); se_text_copy(d.vertex,w.document.vertex); ss_parameters_copy(d.parameters,w.document.parameters);
        se_text_copy(d.fragment_path,w.document.fragment_path); se_text_copy(d.vertex_path,w.document.vertex_path);
        if d.fragment_path[0] == 0 { se_text_copy(d.fragment_path,w.document.path); }
        if d.vertex_path[0] == 0 { se_text_copy(d.vertex_path,w.document.path); }
        if w.document.state[10] != 0 {
            prep :: SSPreprocessor := ss_preprocessor(); vertex :: String := "";
            if w.document.vertex[0] != 0 { vertex = ss_expand(prep,se_text_read(w.document.vertex),se_text_read(d.vertex_path),0,0); }
            fragment :: String := ss_expand(prep,se_text_read(w.document.source),se_text_read(d.fragment_path),0,0);
            if vertex.size() > 65536 || fragment.size() > 65536 { throw "Immutable published expanded stages exceed the 64 KiB host resource limit"; }
            if prep.state[0] != w.document.state[10] { throw "Include set changed since compile; rebuild before Apply"; }
            i :: Int64 := 0; while i < prep.state[0] {
                if se_text_read(prep.paths[i]) != se_text_read(w.document.dependencies[i]) || prep.hashes[i] != w.document.dependency_hashes[i] { throw "Include content changed since compile; rebuild before Apply"; } i = i+1;
            }
            se_text_write(d.source,fragment); se_text_write(d.vertex,vertex); d.fragment_path[0] = 0; d.vertex_path[0] = 0;
        }
        raw :: String := ss_document_text(d); bytes :: Array<Int64> := ss_text(600000); se_text_write(bytes,raw);
        relative :: String := vfs.join(se_text_read(host.publish_directory),"host-"+string(ss_hash(bytes))+".vshader");
        if relative.size() > 255 { throw "Published shader reference exceeds 255 bytes"; }
        full :: String := relative; if !vraylib.is_path_absolute(full) { full = vfs.join(assets.root,full); }
        found :: Bool := false; i :: Int64 := 0;
        while i < assets.programs.state[0] { if ss_normal_path(se_text_read(assets.programs.paths[i])) == ss_normal_path(full) { found = true; } i = i+1; }
        if !found && assets.programs.state[0] == 8 { throw "Scene shader registry capacity: 8 published resources"; }
        missing :: Int64 := 0; i = 0;
        while i < w.material.binding_state[0] {
            source :: Int64 := w.material.bindings[i*4+1]-100;
            if source < 0 || source >= host.resource_state[0] { throw "Unknown studio texture reference"; }
            path :: String := se_text_read(host.resources[source]); exists :: Bool := false; j :: Int64 := 0;
            while j < scene.materials.resource_state[0] { if se_text_read(scene.materials.resources[j]) == path { exists = true; } j = j+1; }
            if !exists { missing = missing+1; } i = i+1;
        }
        if missing+scene.materials.resource_state[0] > 64 { throw "Scene texture reference capacity: 64"; }
        # Content-addressed immutable full resource is written before scene mutation.
        if !vfs.exists(full) { if !vfs.write_atomic(full,raw) { throw "Cannot publish scene shader resource"; } }
        else { existing := vfs.read(full); if existing == null || existing != raw { throw "Shader resource hash collision"; } }
        program :: Int64 := se_program_resolve(assets.programs,app,full);
        se_material_copy(scene.materials.instances[id%128],w.material);
        m :: MaterialInstance := scene.materials.instances[id%128]; m.state[0] = program+3; m.state[1] = id%128+1;
        i = 0; while i < m.binding_state[0] {
            source :: Int64 := m.bindings[i*4+1]-100;
            m.bindings[i*4+1] = se_material_resource(scene.materials,se_text_read(host.resources[source])); i = i+1;
        }
        se_text_write(scene.materials.paths[id%128],relative); se_record(history,scene);
    }; return true;
}
fn se_host_dialog(app :: vraylib.App,host :: SEWorkspaceHost,ui :: SEUI,scene :: SEScene,history :: SEHistory,path :: Array<Int64>,document :: SEDocument,tools :: SEWorldTools) -> Bool {
    if host.state[0] == 0 { return false; }
    w :: SSWorkspace := host.workspace;
    vraylib.draw_rectangle(0,0,app.width(),app.height(),vraylib.rgba(0,0,0,170));
    x :: Float64 := vmath.max(8.0,(float64(app.width())-440.0)*0.5); y :: Float64 := float64(app.height())*0.35;
    se_panel(se_rect(x,y,424.0,152.0),"Unsaved shader / material / graph / pipeline / layout");
    se_draw_text("Save studio resources before continuing?",int64(x+12.0),int64(y+40.0),16,vraylib.WHITE);
    choice :: Int64 := -1;
    if se_button("Save",se_rect(x+12.0,y+84.0,124.0,30.0),true) { choice = 1; }
    if se_button("Discard",se_rect(x+148.0,y+84.0,124.0,30.0),true) { choice = 2; }
    if se_button("Cancel",se_rect(x+284.0,y+84.0,124.0,30.0),true) || vraylib.is_key_pressed(vraylib.KEY_ESCAPE) { choice = 0; }
    return se_host_choice(app,host,ui,scene,history,path,document,choice);
}
fn se_host_choice(app :: vraylib.App,host :: SEWorkspaceHost,ui :: SEUI,scene :: SEScene,history :: SEHistory,path :: Array<Int64>,document :: SEDocument,choice :: Int64) -> Bool {
    if host.state[0] == 0 || choice < 0 { return false; }
    if choice == 0 { host.state[0] = 0; host.state[5] = 0; host.state[6] = -1; host.pending[0] = 0; return true; }
    if choice > 0 {
        if choice == 1 { if !se_host_save(host) { return false; } }
        if host.state[0] == 2 { se_host_finish(app,host); }
        else {
            host.state[0] = 0;
            if host.state[5] == 2 { ui.state[37] = 1; }
            else { se_document_request(ui,scene,history,path,document,3,""); }
            host.state[5] = 1;
        }
    } return true;
}
fn se_host_close(host :: SEWorkspaceHost,assets :: SEAssets) -> Bool {
    layout_saved :: Bool := se_host_flush_layout(host); se_host_pipeline_close(host.workspace.pipeline);
    ss_document_close(host.workspace.document); if !ss_document_collect(host.workspace.document) { throw "Studio program retirement is blocked"; }
    ss_document_close(host.workspace.pass_document); if !ss_document_collect(host.workspace.pass_document) { throw "Pipeline program retirement is blocked"; }
    ss_document_close(host.publication); ss_document_collect(host.publication);
    se_programs_close(assets.programs); if !layout_saved { throw "Host shutdown layout save failed"; } return true;
}
fn se_host_live_bank(tokens :: Array<Int64>) -> Array<Int64> {
    live :: Array<Int64> := []; live.reserve(tokens.size()); i :: Int64 := 0;
    while i < tokens.size() { if tokens[i] != 0 { live.push(tokens[i]); } i = i+1; } return live;
}
fn se_host_pipeline_close(p :: SSPipeline) -> Bool {
    @bounded<1048576> region host_pipeline_close {
        live :: Array<Int64> := se_host_live_bank(p.tokens);
        if live.size() != 0 { owner :: vraylib.App := vraylib.App(p.state[3]);
            if !owner.valid() { throw "Pipeline target owner is unavailable"; }
            if !owner.can_release(live) { throw "Pipeline targets remain active or externally referenced"; }
            if !owner.release_bank(live) { throw "Pipeline target bank release failed"; }
        }
        i :: Int64 := 0; while i < p.tokens.size() { p.tokens[i] = 0; p.outputs[i] = 0; i = i+1; } p.state[2] = 0;
    }; return true;
}
fn se_host_pipeline_prepare(app :: vraylib.App,host :: SEWorkspaceHost) -> Bool {
    w :: SSWorkspace := host.workspace; p :: SSPipeline := w.pipeline; size :: Int64 := w.state[15];
    if w.state[24] == 0 { return false; }
    base_width :: Int64 := se_host_pipeline_base_extent(host,0); base_height :: Int64 := se_host_pipeline_base_extent(host,1);
    if p.state[2] == p.state[1] && p.state[3] == app.token && p.state[4] == base_width*8192+base_height { return false; }
    se_host_pipeline_budget(host); ss_pipeline_validate(p);
    @bounded<16777216> region host_pipeline_prepare {
        old :: Array<Int64> := se_host_live_bank(p.tokens);
        if old.size() != 0 { owner :: vraylib.App := vraylib.App(p.state[3]); if !owner.valid() { throw "Pipeline target owner is unavailable"; } if !owner.can_release(old) { throw "Pipeline targets remain active or externally referenced"; } }
        candidates :: Array<Int64> := ss_ints(16);
        try {
            i :: Int64 := 0; while i < p.state[0] {
                width :: Int64 := p.meta[i*10+5]; height :: Int64 := p.meta[i*10+6];
                if width == 0 { width = int64(vmath.max(1.0,float64(base_width/p.meta[i*10+7]))); }
                if height == 0 { height = int64(vmath.max(1.0,float64(base_height/p.meta[i*10+7]))); }
                target :: vraylib.RenderTarget := vraylib.RenderTarget(0,app.token);
                if p.meta[i*10+8] == 1 { target = app.float_target(width,height); } else { target = app.target(width,height); }
                candidates[i] = target.token; i = i+1;
            }
            se_host_pipeline_close(p);
            i = 0; while i < 16 { p.tokens[i] = candidates[i]; candidates[i] = 0; i = i+1; }
            p.state[2] = p.state[1]; p.state[3] = app.token; p.state[4] = base_width*8192+base_height;
        } finally { live :: Array<Int64> := se_host_live_bank(candidates); if live.size() != 0 { app.release_bank(live); } }
    }; return true;
}
fn se_host_bind(host :: SEWorkspaceHost,scene :: SEScene,history :: SEHistory,bank :: SEAssets,path :: String) -> Bool {
    w :: SSWorkspace := host.workspace; ss_material_migrate(w.material,w.document);
    parameter :: Int64 := se_host_sampler(host); if parameter < 1 { throw "Define a scalar sampler2D parameter in the shader resource before binding a texture"; }
    relative :: String := se_ui_asset_relative(bank.root,path); t :: vraylib.Texture := se_material_texture(bank,relative);
    id :: Int64 := se_host_resource(host,relative);
    ss_material_bind_texture(w.material,parameter,id,t); return true;
}
fn se_host_sampler(host :: SEWorkspaceHost) -> Int64 {
    p :: SSParameters := host.workspace.document.parameters; count :: Int64 := 0; i :: Int64 := 0;
    while i < p.state[0] { if p.meta[i*8+1] == 35678 && p.meta[i*8+2] == 1 { count = count+1; } i = i+1; }
    if count == 0 { return -1; }
    selected :: Int64 := host.state[7]%count; i = 0;
    while i < p.state[0] { if p.meta[i*8+1] == 35678 && p.meta[i*8+2] == 1 { if selected == 0 { return p.meta[i*8]; } selected = selected-1; } i = i+1; }
    return -1;
}
fn se_host_sampler_button(host :: SEWorkspaceHost,bounds :: vraylib.Rectangle,enabled :: Bool) -> Bool {
    id :: Int64 := se_host_sampler(host); if id < 1 || bounds.width < ss_scale(700.0) { return false; }
    p :: SSParameters := host.workspace.document.parameters; slot :: Int64 := ss_parameter_slot(p,id);
    label :: String := "Sampler: "+se_text_read(p.names[slot]);
    box :: vraylib.Rectangle := ss_layout_region(host.workspace.layout,bounds,1);
    if se_button(se_fit(label,ss_scale(178.0),13),se_rect(box.x+box.width-ss_scale(256.0),box.y+ss_scale(5.0),ss_scale(194.0),ss_scale(29.0)),enabled) { host.state[7] = host.state[7]+1; }
    return true;
}
fn se_host_resource(host :: SEWorkspaceHost,path :: String) -> Int64 {
    i :: Int64 := 0; while i < host.resource_state[0] { if se_text_read(host.resources[i]) == path { return i+100; } i = i+1; }
    if i == 64 { throw "Studio texture reference capacity: 64"; }
    se_text_write(host.resources[i],path); host.resource_state[0] = i+1; return i+100;
}
fn se_host_selected_material(host :: SEWorkspaceHost,scene :: SEScene,bank :: SEAssets,id :: Int64) -> Bool {
    if !scene.valid(id) { return false; }
    if scene.kind(id) != 1 { return false; }
    resource :: String := se_text_read(scene.materials.paths[id%128]); if resource != "" {
        if !vraylib.is_path_absolute(resource) { resource = vfs.join(bank.root,resource); }
        se_program_resolve(bank.programs,bank.app,resource);
    }
    m :: MaterialInstance := host.workspace.material; se_material_copy(m,scene.materials.instances[id%128]);
    i :: Int64 := 0; while i < m.binding_state[0] {
        source :: Int64 := m.bindings[i*4+1]-100; if source < 0 || source >= scene.materials.resource_state[0] { throw "Unknown selected material texture"; }
        path :: String := se_text_read(scene.materials.resources[source]); resource :: Int64 := se_host_resource(host,path);
        ss_material_bind_texture(m,m.bindings[i*4],resource,se_material_texture(bank,path)); i = i+1;
    }
    p :: SSParameters := host.workspace.document.parameters; i = 0;
    while i < p.state[0] { s :: Int64 := ss_material_compatible_slot(m,p.meta[i*8],p.meta[i*8+1],p.meta[i*8+2]); if s >= 0 { j :: Int64 := 0; while j < p.meta[i*8+3] { ss_parameter_set(host.workspace.document,p.meta[i*8],j,m.values[s*64+j]); j = j+1; } } i = i+1; }
    se_host_checkpoint(host);
    return true;
}
fn se_host_viewport(host :: SEWorkspaceHost,ui :: SEUI,width :: Int64,height :: Int64,legacy :: Bool) -> vraylib.Rectangle {
    old :: vraylib.Rectangle := se_viewport(ui,width,height); if legacy { return old; }
    s :: Float64 := host.workspace.layout.values[3]; nav :: Float64 := 38.0*s;
    if host.workspace.layout.state[4] != 0 { return se_rect(0.0,nav+42.0,float64(width),vmath.max(1.0,float64(height)-nav-42.0)); }
    left :: Float64 := vmath.clamp(ui.numbers[0]*s,150.0*s,vmath.min(400.0*s,float64(width)*0.3));
    right :: Float64 := vmath.clamp(host.workspace.layout.values[1]*s,180.0*s,vmath.min(600.0*s,float64(width)*0.33));
    bottom :: Float64 := vmath.clamp(host.workspace.layout.values[2]*s,100.0*s,vmath.max(100.0*s,float64(height)-nav-180.0));
    return se_rect(left+4.0,nav+46.0,vmath.max(64.0,float64(width)-left-right-8.0),vmath.max(64.0,float64(height)-nav-bottom-50.0));
}
fn se_host_splitters(host :: SEWorkspaceHost,ui :: SEUI,width :: Int64,height :: Int64,viewport :: vraylib.Rectangle) -> Bool {
    l :: SSLayout := host.workspace.layout;
    mouse :: vraylib.Vector2 := vraylib.get_mouse_position(); s :: Float64 := l.values[3];
    if vraylib.is_key_pressed(vraylib.KEY_F11) { l.state[4] = 1-l.state[4]; l.state[6] = 1; }
    if l.state[4] != 0 { return false; }
    if vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) && mouse.y > viewport.y {
        if vmath.abs(mouse.x-viewport.x) < 4.0 { ui.state[16] = 1; }
        if vmath.abs(mouse.x-viewport.x-viewport.width) < 4.0 { ui.state[17] = 1; }
        if vmath.abs(mouse.y-viewport.y-viewport.height) < 4.0 && mouse.x > viewport.x { ui.state[18] = 1; }
    }
    if vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) {
        if ui.state[16] != 0 { ui.numbers[0] = (mouse.x-4.0)/s; }
        if ui.state[17] != 0 { l.values[1] = vmath.clamp((float64(width)-mouse.x-4.0)/s,180.0,600.0); l.state[6] = 1; }
        if ui.state[18] != 0 { l.values[2] = vmath.clamp((float64(height)-mouse.y-4.0)/s,0.0,480.0); l.state[6] = 1; }
    } else { ui.state[16] = 0; ui.state[17] = 0; ui.state[18] = 0; if l.state[6] != 0 { se_host_flush_layout(host); } }
    return true;
}
fn se_host_rendering_draw(app :: vraylib.App,host :: SEWorkspaceHost,bank :: SEAssets,bounds :: vraylib.Rectangle) -> Bool {
    w :: SSWorkspace := host.workspace;
    vraylib.draw_rectangle_rec(bounds,ss_color(3));
    bar :: vraylib.Rectangle := ss_layout_region(w.layout,bounds,1); vraylib.draw_rectangle_rec(bar,ss_color(1));
    se_draw_heading("Rendering / ordered pass list",int64(bar.x+ss_scale(12.0)),int64(bar.y+ss_scale(10.0)),ss_font_size(),ss_color(6));
    ss_theme_input[0] = int64(!ss_workspace_input_captured(w));
    source :: vraylib.Rectangle := ss_layout_region(w.layout,bounds,2); if source.width > 0.0 { se_host_pipeline_panel(app,host,bank,source); }
    preview :: vraylib.Rectangle := ss_layout_region(w.layout,bounds,3);
    if preview.width > 0.0 && preview.height > 0.0 {
        vraylib.draw_rectangle_rec(preview,ss_color(0)); se_draw_heading("PIPELINE OUTPUT / linear RGBA",int64(preview.x+ss_scale(8.0)),int64(preview.y+ss_scale(8.0)),ss_font_size(),ss_color(5));
        token :: Int64 := w.state[25]; if w.state[13] == 1 && w.state[26] != 0 { token = w.state[2]; } if w.state[13] == 2 && w.state[26] != 0 { token = w.state[3]; }
        if token != 0 { t :: vraylib.RenderTarget := vraylib.RenderTarget(token,app.token); t.texture().draw_region(se_rect(0.0,0.0,float64(t.width()),float64(t.height())),ss_workspace_preview_box(preview),vraylib.vec2(0.0,0.0),0.0,vraylib.WHITE); }
    }
    inspector :: vraylib.Rectangle := ss_layout_region(w.layout,bounds,4);
    if inspector.width > 0.0 && inspector.height > 0.0 { clip :: vraylib.Clip := app.clip(inspector); se_clip_start(clip,inspector); try { vraylib.draw_rectangle_rec(inspector,ss_color(1)); ss_workspace_preview_controls(w,inspector); } finally { se_clip_finish(clip); } }
    ss_workspace_drawer(app,w,ss_layout_region(w.layout,bounds,5)); ss_theme_input[0] = 1; ss_workspace_dialog_draw(w,bounds); return true;
}
fn se_host_pipeline_budget(host :: SEWorkspaceHost) -> Bool {
    w :: SSWorkspace := host.workspace; p :: SSPipeline := w.pipeline;
    if w.state[24] == 0 { return true; }
    return se_host_pipeline_budget_for(host,p);
}
fn se_host_pipeline_budget_for(host :: SEWorkspaceHost,p :: SSPipeline) -> Bool {
    w :: SSWorkspace := host.workspace;
    total :: Int64 := 0; i :: Int64 := 0; while i < p.state[0] {
        divisor :: Int64 := p.meta[i*10+7]; if divisor < 1 { throw "Invalid pipeline resolution divisor"; }
        width :: Int64 := p.meta[i*10+5]; height :: Int64 := p.meta[i*10+6];
        if width == 0 { width = int64(vmath.max(1.0,float64(se_host_pipeline_base_extent(host,0)/divisor))); }
        if height == 0 { height = int64(vmath.max(1.0,float64(se_host_pipeline_base_extent(host,1)/divisor))); }
        if width > 512 || height > 512 { throw "Host pipeline targets are bounded to 512 per axis"; }
        bytes :: Int64 := 8; if p.meta[i*10+8] == 1 { bytes = 20; }
        total = total+width*height*bytes; i = i+1;
    }
    if total > 67108864 { throw "Host pipeline target budget exceeds 64 MiB including depth"; }
    return true;
}
fn se_host_pipeline_candidate(p :: SSPipeline) -> SSPipeline {
    c :: SSPipeline := SSPipeline(ss_ints(160),ss_ints(5),ss_ints(16),ss_ints(16),ss_ints(32),p.defaults,ss_floats(3));
    i :: Int64 := 0; while i < 160 { c.meta[i] = p.meta[i]; i = i+1; } c.state[0] = p.state[0]; return c;
}
fn se_host_render_document(host :: SEWorkspaceHost,id :: Int64) -> SSDocument {
    if host.workspace.documents.size() != 10 || id < 1 || id > 10 { throw "Rendering document registry requires IDs 1..10"; }
    slot :: Int64 := 0; while slot < 10 { if host.workspace.document_ids[slot] == id { return host.workspace.documents[slot]; } slot = slot+1; }
    throw "Rendering document ID is not registered";
}
fn se_host_render_name(host :: SEWorkspaceHost,id :: Int64) -> String {
    if id == 1 { return "Editable #1"; } if id == 2 { return "Identity #2"; }
    d :: SSDocument := se_host_render_document(host,id); return vfs.stem(se_text_read(d.path))+" #"+string(id);
}
fn se_host_pipeline_validate(host :: SEWorkspaceHost,p :: SSPipeline) -> Bool {
    ss_pipeline_validate(p); se_host_pipeline_budget_for(host,p); i :: Int64 := 0;
    while i < p.state[0] { id :: Int64 := p.meta[i*10+1]; d :: SSDocument := se_host_render_document(host,id);
        if p.meta[i*10+4] != 0 {
            shader :: vraylib.Shader := ss_program(d); if !shader.valid() || shader.session != host.workspace.state[0] { throw "Pass shader #"+string(id)+" is not loaded and compiled in this host (token "+string(shader.token)+", session "+string(shader.session)+", expected "+string(host.workspace.state[0])+")"; }
            flags :: Int64 := ss_pipeline_sampler_flags(shader); if flags%2 != 1 { throw "Pass shader needs an active scalar inputTexture sampler2D"; }
            if p.meta[i*10+3] == -2 { if flags != 1 { throw "Pass shader requires inputTexture1; choose its second input"; } }
            else { if flags != 3 { throw "Pass shader has no active inputTexture1; choose absent"; } }
            ss_material_validate_for(d,p.defaults,true);
        } i = i+1;
    } return true;
}
fn se_host_pipeline_publish(host :: SEWorkspaceHost,c :: SSPipeline) -> Bool {
    se_host_pipeline_validate(host,c); return se_host_pipeline_commit(host,c);
}
fn se_host_pipeline_commit(host :: SEWorkspaceHost,c :: SSPipeline) -> Bool {
    p :: SSPipeline := host.workspace.pipeline;
    i :: Int64 := 0; while i < 160 { p.meta[i] = c.meta[i]; i = i+1; } p.state[0] = c.state[0]; p.state[1] = p.state[1]+1;
    host.workspace.state[26] = 0; if p.state[0] == 0 { host.workspace.state[24] = 0; } return true;
}
# Edits are descriptor transactions. GPU target replacement occurs next preframe.
# kind: 0 Add, 1 shader, 2/3 inputs, 4 divisor, 5 format, 6 Remove, 7/8 Up/Down,
# 9 enable, 10 capture, 11 square extent (0 relative, 128/256/512 fixed).
fn se_host_pipeline_edit(host :: SEWorkspaceHost,kind :: Int64,id :: Int64,value :: Int64) -> Bool {
    success :: Bool := false;
    @bounded<16777216> region host_pass_edit { try {
        c :: SSPipeline := se_host_pipeline_candidate(host.workspace.pipeline); slot :: Int64 := ss_pipeline_slot(c,id);
        if kind == 0 {
            input :: Int64 := -1; if c.state[0] > 0 { input = c.meta[(c.state[0]-1)*10]; }
            ss_pipeline_add(c,id,value,input,-2,0,0,1,1); slot = c.state[0]-1;
        } else { if slot < 0 { throw "Unknown pass ID"; } }
        b :: Int64 := slot*10;
        if kind == 0 || kind == 1 {
            c.meta[b+1] = value; d :: SSDocument := se_host_render_document(host,value); flags :: Int64 := ss_pipeline_sampler_flags(ss_program(d));
            if flags == 3 { if c.meta[b+3] == -2 { c.meta[b+3] = -1; } } else { c.meta[b+3] = -2; }
        }
        if kind == 2 || kind == 3 { c.meta[b+kind] = value; }
        if kind == 4 { if value < 1 || value > 16 { throw "Divisor must be 1..16"; } c.meta[b+7] = value; c.meta[b+5] = 0; c.meta[b+6] = 0; }
        if kind == 5 { if value < 0 || value > 1 { throw "Format must be RGBA8 or RGBA32F"; } c.meta[b+8] = value; }
        if kind == 6 { i :: Int64 := b; while i < (c.state[0]-1)*10 { c.meta[i] = c.meta[i+10]; i = i+1; } while i < c.state[0]*10 { c.meta[i] = 0; i = i+1; } c.state[0] = c.state[0]-1; }
        if kind == 7 || kind == 8 {
            other :: Int64 := slot-1; if kind == 8 { other = slot+1; } if other < 0 || other >= c.state[0] { throw "Pass is already at the list boundary"; }
            i :: Int64 := 0; while i < 10 { saved :: Int64 := c.meta[b+i]; c.meta[b+i] = c.meta[other*10+i]; c.meta[other*10+i] = saved; i = i+1; }
        }
        if kind == 9 || kind == 10 { if value < 0 || value > 1 { throw "Pass toggle must be 0 or 1"; } field :: Int64 := 4; if kind == 10 { field = 9; } c.meta[b+field] = value; }
        if kind == 11 { if value != 0 && value != 128 && value != 256 && value != 512 { throw "Host fixed extent is 128, 256 or 512"; } c.meta[b+5] = value; c.meta[b+6] = value; }
        se_host_pipeline_publish(host,c); success = true;
    } catch (error) { se_text_write(host.workspace.log,string(error)); ss_workspace_diagnostics(host.workspace,host.workspace.log); } }; return success;
}
fn se_host_pipeline_next_document(host :: SEWorkspaceHost,current :: Int64) -> Int64 {
    next :: Int64 := current; attempts :: Int64 := 0;
    while attempts < 10 { next = next%10+1; d :: SSDocument := se_host_render_document(host,next);
        if ss_program(d).valid() { if ss_pipeline_sampler_flags(ss_program(d))%2 == 1 { return next; } } attempts = attempts+1;
    } return 2;
}
fn se_host_pipeline_next_input(p :: SSPipeline,slot :: Int64,current :: Int64) -> Int64 {
    if slot == 0 { return -1; } if current == -1 || current == -2 { return p.meta[0]; }
    previous :: Int64 := ss_pipeline_slot(p,current); if previous < 0 || previous+1 >= slot { return -1; } return p.meta[(previous+1)*10];
}
fn se_host_pipeline_queue(host :: SEWorkspaceHost,kind :: Int64,id :: Int64,value :: Int64) -> Bool {
    host.render_state[0] = 1; host.render_state[1] = kind; host.render_state[2] = id; host.render_state[3] = value; return true;
}
fn se_host_find_program(bank :: SEAssets,path :: String) -> Int64 {
    i :: Int64 := 0; while i < bank.programs.state[0] { if ss_normal_path(se_text_read(bank.programs.paths[i])) == ss_normal_path(path) { return i; } i = i+1; } return -1;
}
fn se_host_scene_resources(scene :: SEScene,bank :: SEAssets) -> Bool {
    i :: Int64 := 0; while i < 128 { if scene.meta[i*8] != 0 && scene.meta[i*8+3] == 1 {
        path :: String := se_text_read(scene.materials.paths[i]); if path != "" {
            if !vraylib.is_path_absolute(path) { path = vfs.join(bank.root,path); }
            se_program_resolve(bank.programs,bank.app,path); se_material_resolve_textures(scene.materials,scene.materials.instances[i],bank);
        }
    } i = i+1; } return true;
}
fn se_host_pipeline_input_extent(host :: SEWorkspaceHost,p :: SSPipeline,id :: Int64,axis :: Int64) -> Int64 {
    current :: Int64 := id; depth :: Int64 := 0; while depth < 16 { if current == -1 { return se_host_pipeline_base_extent(host,axis); }
        slot :: Int64 := ss_pipeline_slot(p,current); if slot < 0 { throw "Unknown pipeline extent input"; }
        if p.meta[slot*10+4] == 0 { current = p.meta[slot*10+2]; }
        else { extent :: Int64 := p.meta[slot*10+5+axis]; if extent > 0 { return extent; } return int64(vmath.max(1.0,float64(se_host_pipeline_base_extent(host,axis)/p.meta[slot*10+7]))); }
        depth = depth+1;
    } throw "Pipeline extent dependency depth";
}
fn se_host_pipeline_base_extent(host :: SEWorkspaceHost,axis :: Int64) -> Int64 {
    if host.workspace.state[56] == 0 { return host.workspace.state[15]; }
    target :: vraylib.RenderTarget := vraylib.RenderTarget(host.workspace.state[56],host.workspace.state[0]);
    if axis == 0 { return target.width(); } return target.height();
}
fn se_host_pipeline_context(host :: SEWorkspaceHost) -> Bool {
    p :: SSPipeline := host.workspace.pipeline; i :: Int64 := 0;
    while i < p.state[0] { if p.meta[i*10+4] != 0 {
        d :: SSDocument := se_host_render_document(host,p.meta[i*10+1]); name :: String := vfs.basename(se_text_read(d.path)); axis :: Int64 := -1;
        if name == "blur-horizontal.vshader" { axis = 0; } if name == "blur-vertical.vshader" { axis = 1; }
        if axis >= 0 { width :: Int64 := se_host_pipeline_input_extent(host,p,p.meta[i*10+2],axis); j :: Int64 := 0;
            while j < d.parameters.state[0] { if se_text_read(d.parameters.names[j]) == "texelStep" { ss_parameter_set(d,d.parameters.meta[j*8],0,1.0/float64(width)); } j = j+1; }
        }
    } i = i+1; } return true;
}
fn se_host_pipeline_load(app :: vraylib.App,host :: SEWorkspaceHost,bank :: SEAssets,path :: String) -> Bool {
    success :: Bool := false; previous_resolution :: Int64 := host.workspace.state[15];
    @bounded<67108864> region host_pipeline_library {
        base :: Int64 := bank.programs.state[0]; staged_count :: Int64 := 0;
        paths :: Array := []; ids :: Array<Int64> := ss_ints(10); mapping :: Array<Int64> := ss_ints(256); slots :: Array<Int64> := ss_ints(10);
        i :: Int64 := 0; while i < 10 { paths.push(ss_text(2048)); mapping[i+1] = i+1; slots[i] = -1; i = i+1; }
        try {
            source :: String := path; if !vraylib.is_path_absolute(source) {
                if vfs.exists(source) || vfs.exists(source+".host") { source = vfs.join(vraylib.get_working_directory(),source); }
                else { source = vfs.join(bank.root,source); }
            }
            raw := vfs.read(source+".host"); if raw == null { raw = vfs.read(source); } if raw == null { throw "Cannot read rendering pipeline"; }
            rows :: Array := ss_split(raw,"\n"); builder :: Int64 := vcore.sb_create(); count :: Int64 := 0;
            if rows[0] == "VMGE_RENDER_PIPELINE|1" {
                row :: Int64 := 1; while row < rows.size() { line :: String := rows[row]; if line != "" && line[0] == "D" {
                    f :: Array := ss_split(line,"|"); if f.size() != 3 || count == 10 { throw "Invalid rendering document ledger"; }
                    id :: Int64 := ss_integer(f[1]); if id < 1 || id > 10 || id == 2 { throw "Invalid rendering document ID"; }
                    j :: Int64 := 0; while j < count { if ids[j] == id { throw "Duplicate rendering document ID"; } j = j+1; }
                    ids[count] = id; ss_unhex(paths[count],f[2]); count = count+1;
                } else { vcore.sb_append(builder,line+"\n"); } row = row+1; }
            } else { vcore.sb_append(builder,raw); }
            c :: SSPipeline := se_host_pipeline_candidate(host.workspace.pipeline); ss_pipeline_parse(c,vcore.sb_build(builder));
            # Shipped bloom uses gallery IDs 201..205. Reuse bank slots, not gallery constructors.
            i = 0; while i < c.state[0] {
                id :: Int64 := c.meta[i*10+1];
                if id >= 201 && id <= 205 {
                    found :: Bool := false; j :: Int64 := 0; while j < count { if ids[j] == id { found = true; } j = j+1; }
                    if !found { if count == 10 { throw "Rendering document ledger capacity"; }
                        name :: String := "exposure-tonemap.vshader"; if id == 202 { name = "bloom-extract.vshader"; } if id == 203 { name = "blur-horizontal.vshader"; } if id == 204 { name = "blur-vertical.vshader"; } if id == 205 { name = "bloom-composite.vshader"; }
                        ids[count] = id; se_text_write(paths[count],vfs.join(vfs.dirname(source),name)); count = count+1;
                    }
                }
                i = i+1;
            }
            missing :: Int64 := 0; i = 0;
            # Stable gallery/ledger load order keeps remapped IDs deterministic.
            i = 0; while i < count { j :: Int64 := i+1; while j < count {
                if ids[j] < ids[i] { saved :: Int64 := ids[i]; ids[i] = ids[j]; ids[j] = saved;
                    text :: String := se_text_read(paths[i]); se_text_copy(paths[i],paths[j]); se_text_write(paths[j],text);
                } j = j+1;
            } i = i+1; } i = 0;
            while i < count {
                full :: String := se_text_read(paths[i]); if !vraylib.is_path_absolute(full) { full = vfs.join(bank.root,full); } full = ss_normal_path(full); se_text_write(paths[i],full);
                editable :: String := se_text_read(host.workspace.document.path); if !vraylib.is_path_absolute(editable) { editable = vfs.join(vraylib.get_working_directory(),editable); }
                if ids[i] == 1 && full == ss_normal_path(editable) { slots[i] = -2; mapping[ids[i]] = 1; }
                else {
                    slots[i] = se_host_find_program(bank,full);
                    if slots[i] < 0 {
                        j :: Int64 := 0; while j < i { if se_text_read(paths[j]) == full && slots[j] >= base { slots[i] = slots[j]; } j = j+1; }
                        if slots[i] < 0 { slots[i] = base+missing; missing = missing+1; }
                    }
                    mapping[ids[i]] = slots[i]+3;
                } i = i+1;
            }
            if base+missing > 8 { throw "Bloom/pass library needs more than the remaining 8-slot shader bank capacity; history is never evicted"; }
            i = 0; while i < count {
                if slots[i] == base+staged_count {
                    d :: SSDocument := bank.programs.documents[slots[i]]; ss_document_load(d,se_text_read(paths[i]));
                    if !ss_compile(app,d) { throw "Pass library compile failed: "+se_text_read(d.log); } staged_count = staged_count+1;
                } i = i+1;
            }
            i = 0; while i < c.state[0] { id :: Int64 := c.meta[i*10+1]; if id < 1 || id >= mapping.size() || mapping[id] == 0 { throw "Pipeline has no resolvable shader resource for document #"+string(id); } c.meta[i*10+1] = mapping[id]; i = i+1; }
            host.workspace.state[15] = 512; se_host_pipeline_validate(host,c);
            i = 0; while i < count { if slots[i] >= base { se_text_copy(bank.programs.paths[slots[i]],paths[i]); } i = i+1; }
            bank.programs.state[0] = base+missing; se_host_pipeline_commit(host,c);
            host.workspace.state[24] = 1; host.workspace.state[15] = 512; host.workspace.state[30] = 0; host.workspace.layout.state[3] = 3; success = true;
        } catch (error) {
            host.workspace.state[15] = previous_resolution;
            bank.programs.state[0] = base;
            i = 0; while i < staged_count { d :: SSDocument := bank.programs.documents[base+i]; ss_document_close(d); ss_document_collect(d); i = i+1; }
            se_text_write(host.workspace.log,string(error)); ss_workspace_diagnostics(host.workspace,host.workspace.log);
            out("Rendering load: ",string(error));
        }
    }; return success;
}
fn se_host_pipeline_save(host :: SEWorkspaceHost,bank :: SEAssets) -> Bool { return se_host_pipeline_save_root(host,bank.root); }
fn se_host_pipeline_save_root(host :: SEWorkspaceHost,root :: String) -> Bool {
    success :: Bool := false;
    @bounded<16777216> region host_pipeline_checkpoint { try {
        se_host_pipeline_validate(host,host.workspace.pipeline); path :: String := se_text_read(host.pipeline_path);
        b :: Int64 := vcore.sb_create(); vcore.sb_append(b,"VMGE_RENDER_PIPELINE|1\n"+ss_pipeline_text(host.workspace.pipeline)); seen :: Array<Int64> := ss_ints(10); i :: Int64 := 0;
        while i < host.workspace.pipeline.state[0] { id :: Int64 := host.workspace.pipeline.meta[i*10+1]; if id != 2 && seen[id-1] == 0 {
            d :: SSDocument := se_host_render_document(host,id); full :: String := se_text_read(d.path); if !vraylib.is_path_absolute(full) { full = vfs.join(vraylib.get_working_directory(),full); } if !vfs.exists(full) { throw "Save the editable/pass shader before its pipeline"; }
            relative :: String := se_ui_asset_relative(root,full); bytes :: Array<Int64> := ss_text(2048); se_text_write(bytes,relative);
            vcore.sb_append(b,"D|"+string(id)+"|"+se_text_hex(bytes)+"\n"); seen[id-1] = 1;
        } i = i+1; }
        if !ss_pipeline_save(host.workspace.pipeline,path) { throw "Pipeline save failed"; }
        if !vfs.write_atomic(path+".host",vcore.sb_build(b)) { throw "Rendering document ledger save failed"; }
        host.state[2] = host.workspace.pipeline.state[1]; success = true;
    } catch (error) { se_text_write(host.workspace.log,string(error)); ss_workspace_diagnostics(host.workspace,host.workspace.log); } }; return success;
}
fn se_host_pipeline_actions(app :: vraylib.App,host :: SEWorkspaceHost,bank :: SEAssets) -> Bool {
    action :: Int64 := host.render_state[0]; host.render_state[0] = 0;
    if action == 1 { return se_host_pipeline_edit(host,host.render_state[1],host.render_state[2],host.render_state[3]); }
    if action == 2 { return se_host_pipeline_load(app,host,bank,vfs.join(bank.root,"shaders/studio/bloom.vpipeline")); }
    if action == 3 { return se_host_pipeline_save(host,bank); }
    if action == 4 || action == 5 { name :: String := "plasma-grid.vshader"; if action == 5 { name = "lit-material.vshader"; } return se_host_open(app,host,vfs.join(bank.root,"shaders/studio/"+name)); }
    return true;
}
fn se_host_pipeline_panel(app :: vraylib.App,host :: SEWorkspaceHost,bank :: SEAssets,box :: vraylib.Rectangle) -> Bool {
    w :: SSWorkspace := host.workspace; p :: SSPipeline := w.pipeline; clip :: vraylib.Clip := app.clip(box); se_clip_start(clip,box);
    try {
        vraylib.draw_rectangle_rec(box,ss_color(0)); x :: Float64 := box.x+ss_scale(8.0); y :: Float64 := box.y+ss_scale(6.0); width :: Float64 := box.width-ss_scale(16.0);
        label :: String := "Enable pipeline"; if w.state[24] != 0 { label = "Disable pipeline"; }
        if ss_workspace_action(w,label,se_rect(x,y,width*0.34,ss_scale(28.0)),p.state[0] > 0) { w.state[24] = 1-w.state[24]; }
        if ss_workspace_action(w,"Load bloom gallery",se_rect(x+width*0.35,y,width*0.4,ss_scale(28.0)),true) { host.render_state[0] = 2; }
        if ss_workspace_action(w,"Save",se_rect(x+width*0.77,y,width*0.23,ss_scale(28.0)),true) { host.render_state[0] = 3; }
        y = y+ss_scale(34.0);
        if ss_workspace_action(w,"New: "+se_host_render_name(host,host.render_state[4]),se_rect(x,y,width*0.66,ss_scale(28.0)),true) { host.render_state[4] = se_host_pipeline_next_document(host,host.render_state[4]); }
        if ss_workspace_action(w,"Add pass",se_rect(x+width*0.68,y,width*0.32,ss_scale(28.0)),p.state[0] < 16) { id :: Int64 := 1; while ss_pipeline_slot(p,id) >= 0 { id = id+1; } se_host_pipeline_queue(host,0,id,host.render_state[4]); }
        y = y+ss_scale(34.0);
        if ss_workspace_action(w,"Open plasma seed",se_rect(x,y,width*0.49,ss_scale(25.0)),true) { host.render_state[0] = 4; }
        if ss_workspace_action(w,"Open lit material",se_rect(x+width*0.51,y,width*0.49,ss_scale(25.0)),true) { host.render_state[0] = 5; }
        y = y+ss_scale(32.0);
        if ss_theme_input[0] != 0 && se_hover(box) { host.render_state[5] = int64(vmath.clamp(float64(host.render_state[5])-vraylib.get_mouse_wheel_move(),0.0,vmath.max(0.0,float64(p.state[0]-1)))); }
        i :: Int64 := host.render_state[5]; while i < p.state[0] && y < box.y+box.height {
            b :: Int64 := i*10; id :: Int64 := p.meta[b]; third :: Float64 := (width-ss_scale(8.0))/3.0;
            se_draw_heading("Pass #"+string(id),int64(x),int64(y),ss_font_size(),ss_color(4)); y = y+ss_scale(23.0);
            if ss_workspace_action(w,"Shader: "+se_host_render_name(host,p.meta[b+1]),se_rect(x,y,width,ss_scale(26.0)),true) { se_host_pipeline_queue(host,1,id,se_host_pipeline_next_document(host,p.meta[b+1])); }
            y = y+ss_scale(30.0);
            if ss_workspace_action(w,"In0: "+string(p.meta[b+2]),se_rect(x,y,width*0.49,ss_scale(25.0)),true) { se_host_pipeline_queue(host,2,id,se_host_pipeline_next_input(p,i,p.meta[b+2])); }
            d :: SSDocument := se_host_render_document(host,p.meta[b+1]); second :: Bool := false; if ss_program(d).valid() { second = ss_pipeline_sampler_flags(ss_program(d)) == 3; }
            if ss_workspace_action(w,"In1: "+string(p.meta[b+3]),se_rect(x+width*0.51,y,width*0.49,ss_scale(25.0)),second) { se_host_pipeline_queue(host,3,id,se_host_pipeline_next_input(p,i,p.meta[b+3])); }
            y = y+ss_scale(29.0);
            if ss_workspace_action(w,"Div /"+string(p.meta[b+7]),se_rect(x,y,third,ss_scale(25.0)),true) { divisor :: Int64 := p.meta[b+7]*2; if divisor > 16 { divisor = 1; } se_host_pipeline_queue(host,4,id,divisor); }
            label = "RGBA8"; if p.meta[b+8] == 1 { label = "RGBA32F"; }
            if ss_workspace_action(w,label,se_rect(x+third+ss_scale(4.0),y,third,ss_scale(25.0)),true) { se_host_pipeline_queue(host,5,id,1-p.meta[b+8]); }
            if ss_workspace_action(w,"Size "+string(p.meta[b+5]),se_rect(x+(third+ss_scale(4.0))*2.0,y,third,ss_scale(25.0)),true) { extent :: Int64 := 128; if p.meta[b+5] == 128 { extent = 256; } if p.meta[b+5] == 256 { extent = 512; } if p.meta[b+5] == 512 { extent = 0; } se_host_pipeline_queue(host,11,id,extent); }
            y = y+ss_scale(29.0); label = "Bypass"; if p.meta[b+4] == 0 { label = "Enable"; }
            if ss_workspace_action(w,label,se_rect(x,y,third,ss_scale(25.0)),true) { se_host_pipeline_queue(host,9,id,1-p.meta[b+4]); }
            if ss_workspace_action(w,"Capture",se_rect(x+third+ss_scale(4.0),y,third,ss_scale(25.0)),true) { se_host_pipeline_queue(host,10,id,1-p.meta[b+9]); }
            if ss_workspace_action(w,"Inspect",se_rect(x+(third+ss_scale(4.0))*2.0,y,third,ss_scale(25.0)),p.outputs[i] != 0) { w.state[30] = i; w.layout.state[3] = 3; }
            y = y+ss_scale(29.0);
            if ss_workspace_action(w,"Up",se_rect(x,y,third,ss_scale(25.0)),i > 0) { se_host_pipeline_queue(host,7,id,0); }
            if ss_workspace_action(w,"Down",se_rect(x+third+ss_scale(4.0),y,third,ss_scale(25.0)),i+1 < p.state[0]) { se_host_pipeline_queue(host,8,id,0); }
            if ss_workspace_action(w,"Remove",se_rect(x+(third+ss_scale(4.0))*2.0,y,third,ss_scale(25.0)),true) { se_host_pipeline_queue(host,6,id,0); }
            y = y+ss_scale(38.0); i = i+1;
        }
    } finally { se_clip_finish(clip); } return true;
}
fn se_host_toolbar(app :: vraylib.App,host :: SEWorkspaceHost,ui :: SEUI,scene :: SEScene,runtime :: SEScene,cache :: SETransforms,player :: VMGEPlayer,history :: SEHistory,path :: Array<Int64>,document :: SEDocument) -> Bool {
    y :: Float64 := 38.0*host.workspace.layout.values[3];
    vraylib.draw_rectangle_rec(se_rect(0.0,y,float64(app.width()),42.0),ss_color(1));
    editable :: Bool := ui.state[4] == 0 && ui.state[13] == 0;
    labels := ["New","Open","Save","Undo","Redo"]; i :: Int64 := 0;
    while i < 5 {
        if se_button(labels[i],se_rect(6.0+float64(i)*52.0,y+6.0,48.0,29.0),editable) {
            if i == 0 { se_document_request(ui,scene,history,path,document,1,""); }
            if i == 1 { se_document_request(ui,scene,history,path,document,2,""); }
            if i == 2 { se_document_save(ui,scene,history,path,document,false); }
            if i == 3 { se_undo(history,scene); } if i == 4 { se_redo(history,scene); }
        } i = i+1;
    }
    label :: String := "Play"; if ui.state[4] != 0 { label = "Stop"; }
    if se_button(label,se_rect(270.0,y+6.0,48.0,29.0),ui.state[13] == 0 && ui.state[1] == -1 && ui.state[2] == -1) { se_toggle_play(ui,scene,runtime,cache,player,app); }
    if se_button("T/R/S",se_rect(324.0,y+6.0,56.0,29.0),editable) { ui.state[3] = (ui.state[3]+1)%3; }
    if se_button("Focus",se_rect(386.0,y+6.0,56.0,29.0),editable && scene.valid(ui.state[0])) { se_focus(ui,cache,ui.state[0]%128); }
    if se_button("Maximize",se_rect(448.0,y+6.0,84.0,29.0),ui.state[13] == 0) { host.workspace.layout.state[4] = 1-host.workspace.layout.state[4]; host.workspace.layout.state[6] = 1; }
    if se_button("Exit",se_rect(float64(app.width())-62.0,y+6.0,56.0,29.0),editable) { se_document_request(ui,scene,history,path,document,3,""); }
    return true;
}
fn se_host_grid(ui :: SEUI) -> Bool {
    spacing :: Float64 := 1.0; if ui.numbers[5] > 60.0 { spacing = 5.0; } if ui.numbers[5] > 250.0 { spacing = 25.0; }
    i :: Int64 := -20; while i <= 20 {
        c :: vraylib.Color := vraylib.rgba(58,64,74,255); if i%5 == 0 { c = vraylib.rgba(72,79,90,255); }
        p :: Float64 := float64(i)*spacing; extent :: Float64 := 20.0*spacing;
        vraylib.draw_line_3d(vraylib.vec3(p,0.0,-extent),vraylib.vec3(p,0.0,extent),c);
        vraylib.draw_line_3d(vraylib.vec3(-extent,0.0,p),vraylib.vec3(extent,0.0,p),c); i = i+1;
    } return true;
}
fn se_host_probe(app :: vraylib.App,w :: SSWorkspace,bank :: SEAssets,scene :: SEScene,cubes :: Array,slot :: Int64) -> Float64 {
    target :: vraylib.RenderTarget := vraylib.RenderTarget(w.state[1],app.token);
    camera :: vraylib.Camera := vraylib.Camera(w.state[4],app.token,0.0,0.0,vraylib.CAMERA_ORBITAL);
    path :: String := se_text_read(scene.materials.paths[slot]); if !vraylib.is_path_absolute(path) { path = vfs.join(bank.root,path); }
    program :: Int64 := se_program_resolve(bank.programs,app,path);
    target.begin(); try { app.clear(vraylib.BLACK); camera.begin(); try {
        ss_material_draw(se_node_model(scene,slot,bank,cubes),vraylib.math_matrix_identity(),bank.programs.documents[program],scene.materials.instances[slot],0,vraylib.WHITE);
    } finally { camera.end(); } } finally { target.end(); }
    pixels :: Array<Float64> := target.read_pixels(); return pixels[(target.height()/2*target.width()+target.width()/2)*4];
}
fn se_host_document_pixel(app :: vraylib.App,w :: SSWorkspace,d :: SSDocument,m :: MaterialInstance,model :: vraylib.Model) -> Float64 {
    target :: vraylib.RenderTarget := vraylib.RenderTarget(w.state[1],app.token);
    camera :: vraylib.Camera := vraylib.Camera(w.state[4],app.token,0.0,0.0,vraylib.CAMERA_ORBITAL);
    target.begin(); try { app.clear(vraylib.BLACK); camera.begin(); try {
        ss_material_draw(model,vraylib.math_matrix_identity(),d,m,0,vraylib.WHITE);
    } finally { camera.end(); } } finally { target.end(); }
    pixels :: Array<Float64> := target.read_pixels(); return pixels[(target.height()/2*target.width()+target.width()/2)*4];
}
