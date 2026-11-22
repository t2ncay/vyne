use "../ui/code_editor.vy";
use "../shader/graph.vy";
use "../shader/pipeline.vy";
use "../engine/assets.vy";
use "../ui/shader_schema.vy";

# Numeric resource IDs only; this workspace is hosted by the scene's App/session.
# state: session,target,reference,difference,camera,plane,cube,sphere,selected model,
# failed build,diagnostic count,diagnostic overflow,capture request,compare mode,
# frozen,resolution,seen document revision,apply request,param scroll,param slot,
# param component,graph selected,graph dragging,wire source,pipeline enabled,
# output target,reference valid,pixel request,time uniform,resolution uniform,
# pipeline selected,graph next ID,reference resolution,save error.
# 42 hosted, 43 Save, 46 Reset, 47 Save As, 53 Reload; 55 whole-frame capture.
# 44/45 focused parameter shape, 48/49 excerpt line/column, 50 copied include paths,
# 51/52 source-map hashes. values[23] is layout retry time; 24..27 hold tooltip/
# untouched numeric-field state. panel owns small metadata/value history controls.

# Retained Shader Studio owner with editable/pass documents, source/parameter editors, graph/pipeline,
# material, layout, diagnostics, preview/reference buffers, and hosted action/capture state.
#
# Fields:
#     document: Document owner or retained serialized text buffer, as typed below.
#     fragment: Fragment-source editor sharing the document source buffer.
#     vertex: Vertex-source editor sharing the document vertex buffer.
#     layout: Retained layout payload.
#     material: Retained material payload.
#     graph: Retained graph payload.
#     pipeline: Retained pipeline payload.
#     pass_document: Identity/fullscreen pass document used by preview/pipeline operations.
#     documents: Preallocated shader-document registry parallel to document_ids.
#     document_ids: Stable IDs parallel to the shader-document registry.
#     state: Mutable numeric state/counters used by this operation.
#     values: Numeric values or a non-owning view of those values, as typed below.
#     project: VMGE project directory containing shaders, assets, and configuration.
#     layout_path: Retained saved workspace-layout path.
#     diagnostics: Retained numeric diagnostic rows used for source-gutter markers.
#     messages: Retained messages payload.
#     log: Retained UTF-8 compiler/editor diagnostic text.
#     saved_source: Saved fragment-source reset before-image.
#     saved_vertex: Saved vertex-source reset before-image.
#     reference_pixels: Retained linear RGBA reference for preview/difference inspection.
#     parameter_editor: Retained parameter editor payload.
#     diagnostic_stages: Fragment/vertex stage identity parallel to diagnostic rows.
#     visible_diagnostics: Diagnostic row indices selected by current filtering.
#     diagnostic_sources: Numeric GLSL root/include/graph source IDs per diagnostic.
#     diagnostic_targets: Navigation targets resolved from compiler diagnostic locations.
#     diagnostic_nodes: Stable graph node IDs associated with graph diagnostics.
#     diagnostic_path_slots: Indices into cached include-path storage per diagnostic.
#     diagnostic_paths: Packed retained include-path bytes for diagnostics.
#     diagnostic_path_lengths: Byte lengths parallel to cached diagnostic paths.
#     diagnostic_notice: Retained include/source-context notice text.
#     diagnostic_open_path: Retained requested diagnostic-resource path consumed by the host.
#     panel: Retained panel payload.
interface SSWorkspace {
    document :: SSDocument,
    fragment :: SSCodeEditor,
    vertex :: SSCodeEditor,
    layout :: SSLayout,
    material :: MaterialInstance,
    graph :: SSGraph,
    pipeline :: SSPipeline,
    pass_document :: SSDocument,
    documents :: Array,
    document_ids :: Array<Int64>,
    state :: Array<Int64>,
    values :: Array<Float64>,
    project :: Array<Int64>,
    layout_path :: Array<Int64>,
    diagnostics :: Array<Int64>,
    messages :: Array<Int64>,
    log :: Array<Int64>,
    saved_source :: Array<Int64>,
    saved_vertex :: Array<Int64>,
    reference_pixels :: Array<Float64>,
    parameter_editor :: SEUI,
    diagnostic_stages :: Array<Int64>,
    visible_diagnostics :: Array<Int64>,
    diagnostic_sources :: Array<Int64>,
    diagnostic_targets :: Array<Int64>,
    diagnostic_nodes :: Array<Int64>,
    diagnostic_path_slots :: Array<Int64>,
    diagnostic_paths :: Array<Int64>,
    diagnostic_path_lengths :: Array<Int64>,
    diagnostic_notice :: Array<Int64>,
    diagnostic_open_path :: Array<Int64>,
    panel :: SSShaderPanel,
}

# Create parameter-edit UI state with source-editor-compatible theme and retained numeric controls.
#
# Returns:
#     SEUI containing the resulting retained state or payload.
fn ss_workspace_parameter_ui() -> SEUI {
    ui :: SEUI := se_ui(ss_text(255));
    # Reuse otherwise idle preframe asset slots for long path fields and staging.
    # No second SEUI or additional 2048-byte path owner is acquired.
    ui.text = ui.preview_paths;
    ui.edit_buffer = ui.asset_names;
    return ui;
}

# Allocate the complete Shader Studio source/history/schema/preview/graph/pipeline owners and load
# configured documents/layout before frames.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     project (String): VMGE project directory containing shaders, assets, and configuration.
#
# Returns:
#     SSWorkspace containing the resulting retained state or payload.
#
# Errors:
#     Difference shader (with contextual detail).
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn ss_workspace_create(app :: vraylib.App, project :: String) -> SSWorkspace {
    se_theme_load(app, project);
    d :: SSDocument := ss_document();
    path :: String := vmge_option("VYNE_VMGE_SHADER_DOCUMENT", vfs.join(project, "shaders/studio.vshader"));
    if vfs.exists(path) {
        ss_document_load(d, path);
    } else {
        se_text_write(d.path, path);
    }
    pass :: SSDocument := ss_document();
    pass.parameters.state[0] = 0;
    se_text_write(
        pass.source,
        "#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nuniform sampler2D inputTexture;\nvoid main()\{finalColor=texture(inputTexture,fragTexCoord);}\n"
    );
    ss_document_source_change(pass);
    graph :: SSGraph := ss_graph();
    ss_graph_add(graph, 1, 0, 4, "Color", 0);
    ss_graph_add(graph, 2, 7, 4, "Output", 0);
    ss_graph_connect(graph, 2, 0, 1);
    ss_graph_value(graph, 1, 0, 0.25);
    ss_graph_value(graph, 1, 1, 0.55);
    ss_graph_value(graph, 1, 2, 0.8);
    ss_graph_value(graph, 1, 3, 1.0);
    ss_graph_value(graph, 1, 4, 24.0);
    ss_graph_value(graph, 1, 5, 80.0);
    ss_graph_value(graph, 2, 4, 250.0);
    ss_graph_value(graph, 2, 5, 100.0);
    pipeline :: SSPipeline := ss_pipeline();
    ss_pipeline_add(pipeline, 1, 2, -1, -2, 0, 0, 1, 1);
    graph_path :: String := vmge_option("VYNE_VMGE_SHADER_GRAPH", vfs.join(project, "shaders/studio.vgraph"));
    pipeline_path :: String := vmge_option(
        "VYNE_VMGE_SHADER_PIPELINE",
        vfs.join(project, "shaders/studio.vpipeline")
    );
    layout_path :: String := vmge_option("VYNE_VMGE_LAYOUT", vfs.join(project, "shaders/studio.layout"));
    if vfs.exists(graph_path) {
        ss_graph_load(graph, graph_path);
    }
    if vfs.exists(pipeline_path) {
        ss_pipeline_load(pipeline, pipeline_path);
    }
    layout :: SSLayout := ss_layout();
    if !ss_layout_load(layout, layout_path) {
        layout.values[3] = vraylib.math_clamp(vraylib.get_window_scale_dpi().x, 0.75, 3.0);
    }
    state :: Array<Int64> := ss_ints(64);
    state[0] = app.token;
    state[15] = 256;
    state[19] = -1;
    state[21] = -1;
    state[23] = -1;
    state[31] = 3;
    values :: Array<Float64> := ss_floats(32);
    values[18] = 0.7;
    values[19] = 0.45;
    values[20] = 4.0;
    target :: vraylib.RenderTarget := app.float_target(256, 256);
    reference :: vraylib.RenderTarget := app.float_target(256, 256);
    difference :: vraylib.RenderTarget := app.target(256, 256);
    camera :: vraylib.Camera := app.camera(vraylib.vec3(3.0, 2.0, 3.0), vraylib.vec3(0.0, 0.0, 0.0), 45.0);
    white :: vraylib.Image := app.image(1, 1, vraylib.WHITE);
    texture :: vraylib.Texture := app.texture(white);
    white.close();
    plane :: vraylib.Model := app.plane(texture, 2.0, 2.0);
    cube :: vraylib.Model := app.cube(texture);
    native_sphere :: Int64 := vraylib.load_model_from_mesh(vraylib.gen_mesh_sphere(0.8, 32, 32));
    vraylib.managed_adopt(app.token, native_sphere);
    sphere :: vraylib.Model := vraylib.Model(native_sphere, app.token);
    sphere.set_texture(texture, 0);
    state[1] = target.token;
    state[2] = reference.token;
    state[3] = difference.token;
    state[4] = camera.token;
    state[5] = plane.token;
    state[6] = cube.token;
    state[7] = sphere.token;
    compare :: vraylib.ShaderBuild := app.compile_shader(
        null,
        "#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\nuniform sampler2D texture0;\nuniform sampler2D referenceTexture;\nvoid main()\{vec3 d=abs(texture(texture0,fragTexCoord).rgb-texture(referenceTexture,fragTexCoord).rgb);finalColor=vec4(d*4.0,1.0);}\n"
    );
    if compare.success {
        state[34] = compare.shader.token;
    } else {
        throw "Difference shader: " + compare.log;
    }
    docs :: Array := [];
    docs.push(d);
    docs.push(pass);
    workspace :: SSWorkspace := SSWorkspace(
        d,
        ss_code_editor(d.source),
        ss_code_editor(d.vertex),
        layout,
        ss_material(1, 1),
        graph,
        pipeline,
        pass,
        docs,
        [1, 2],
        state,
        values,
        ss_text(2048),
        ss_text(2048),
        ss_ints(512),
        ss_ints(128 * 513),
        ss_text(16384),
        ss_text(65536),
        ss_text(65536),
        ss_floats(512 * 512 * 4),
        ss_workspace_parameter_ui(),
        ss_ints(128),
        ss_ints(512),
        ss_ints(128),
        ss_ints(128),
        ss_ints(128),
        ss_ints(128),
        ss_ints(32 * 2048),
        ss_ints(32),
        ss_text(4096),
        ss_text(2048),
        ss_shader_panel()
    );
    se_text_write(workspace.project, project);
    se_text_write(workspace.layout_path, layout_path);
    se_text_copy(workspace.saved_source, d.source);
    se_text_copy(workspace.saved_vertex, d.vertex);
    ss_compile(app, pass);
    ss_workspace_compile(app, workspace);
    workspace.state[16] = d.state[0];
    workspace.state[36] = ss_hash(d.source);
    workspace.state[37] = ss_hash(d.vertex);
    return workspace;
}

# Resolve the fragment or vertex source editor according to the active source tab.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The existing fragment or vertex editor selected by the source tab.
fn ss_workspace_editor(workspace :: SSWorkspace) -> SSCodeEditor {
    if workspace.layout.state[1] == 1 {
        return workspace.vertex;
    }
    return workspace.fragment;
}

# Compute the source editor's screen rectangle within current workspace splits.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     bounds (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     The computed vraylib.Rectangle value: se_rect(box.x, box.y + ss_scale(32.0), box.width,
#     vmath.max(0.0, box.height - ss_scale(32.0))).
fn ss_workspace_source_box(workspace :: SSWorkspace, bounds :: vraylib.Rectangle) -> vraylib.Rectangle {
    box :: vraylib.Rectangle := ss_layout_region(workspace.layout, bounds, 2);
    return se_rect(box.x, box.y + ss_scale(32.0), box.width, vmath.max(0.0, box.height - ss_scale(32.0)));
}

# Return a retained-document status label reflecting dirty, compile, Apply, or error state.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_workspace_status(workspace :: SSWorkspace) -> String {
    if workspace.state[9] != 0 {
        return "Failed";
    }
    if ss_document_compile_dirty(workspace.document) {
        return "Draft";
    }
    if ss_document_applied(workspace.document) {
        return "Applied";
    }
    return "Compiled";
}

# Diagnostics own byte slots, including messages copied from a transient build log.
# Mesa/NVIDIA forms 0:line(column) and 0(line) are recognized, plus ERROR: 0:line:.

# Parse compiler/editor log text into retained clickable diagnostic rows.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     log (Array<Int64>): Retained UTF-8 compiler/editor diagnostic text.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_diagnostics(workspace :: SSWorkspace, log :: Array<Int64>) -> Bool {
    if workspace.layout.state[3] == 4 {
        workspace.layout.state[3] = 0;
    }
    se_text_copy(workspace.log, log);
    workspace.state[10] = 0;
    workspace.state[11] = 0;
    workspace.panel.state[4] = 0;
    workspace.panel.state[5] = 0;
    workspace.diagnostic_notice[0] = 0;
    start :: Int64 := 0;
    i :: Int64 := 0;
    stage :: Int64 := 0;
    while i <= log[0] {
        if i == log[0] || log[i + 1] == 10 {
            if i > start {
                caption :: String := se_edit_slice(log, start, i);
                if caption == "vertex:" {
                    stage = 1;
                }
                if caption == "fragment:" {
                    stage = 0;
                }
                if caption == "link:" {
                    stage = -1;
                }
                n :: Int64 := workspace.state[10];
                if n >= 128 {
                    workspace.state[11] = 1;
                    break;
                }
                length :: Int64 := int64(vmath.min(512.0, float64(i - start)));
                base :: Int64 := n * 513;
                workspace.messages[base] = length;
                j :: Int64 := 1;
                while j <= length {
                    workspace.messages[base + j] = log[start + j];
                    j = j + 1;
                }
                line :: Int64 := 0;
                column :: Int64 := 1;
                source_id :: Int64 := -1;
                pos :: Int64 := start;
                while pos + 2 < i {
                    if log[pos + 1] >= 48 && log[pos + 1] <= 57 {
                        file_id :: Int64 := 0;
                        while pos < i && log[pos + 1] >= 48 && log[pos + 1] <= 57 {
                            file_id = int64(vmath.min(1000000.0, float64(file_id * 10 + log[pos + 1] - 48)));
                            pos = pos + 1;
                        }
                        if pos < i && (log[pos + 1] == 58 || log[pos + 1] == 40) {
                            pos = pos + 1;
                            candidate :: Int64 := 0;
                            digits :: Int64 := 0;
                            while pos < i && log[pos + 1] >= 48 && log[pos + 1] <= 57 {
                                candidate = int64(vmath.min(
                                    1000000.0,
                                    float64(candidate * 10 + log[pos + 1] - 48)
                                ));
                                digits = digits + 1;
                                pos = pos + 1;
                            }
                            if digits > 0 {
                                source_id = file_id;
                                line = candidate;
                                if pos < i && log[pos + 1] == 40 {
                                    pos = pos + 1;
                                    column = 0;
                                    while pos < i && log[pos + 1] >= 48 && log[pos + 1] <= 57 {
                                        column = int64(vmath.min(
                                            1000000.0,
                                            float64(column * 10 + log[pos + 1] - 48)
                                        ));
                                        pos = pos + 1;
                                    }
                                    if column == 0 {
                                        column = 1;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    pos = pos + 1;
                }
                workspace.diagnostics[n * 4] = line;
                workspace.diagnostics[n * 4 + 1] = column;
                workspace.diagnostics[n * 4 + 2] = start;
                workspace.diagnostics[n * 4 + 3] = length;
                workspace.diagnostic_stages[n] = stage;
                workspace.diagnostic_sources[n] = source_id;
                ss_workspace_resolve_diagnostic(workspace, n);
                workspace.state[10] = n + 1;
            }
            start = i + 1;
        }
        i = i + 1;
    }
    return true;
}

# Associate diagnostic source IDs with fragment/vertex root stages or include/graph contexts.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     stage (Int64): Source stage or preacquired document-bank offset selected by this operation.
#
# Returns:
#     The computed Int64 value: count.
fn ss_workspace_stage_diagnostics(workspace :: SSWorkspace, stage :: Int64) -> Int64 {
    count :: Int64 := 0;
    i :: Int64 := 0;
    while i < workspace.state[10] {
        if workspace.diagnostic_stages[i] == stage && workspace.diagnostic_targets[i] == 0 {
            j :: Int64 := 0;
            while j < 4 {
                workspace.visible_diagnostics[count * 4 + j] = workspace.diagnostics[i * 4 + j];
                j = j + 1;
            }
            count = count + 1;
        }
        i = i + 1;
    }
    return count;
}

# Targets: 0 root stage, 1 include path, 2 graph node, 3 log-only/unresolved.

# Resolve a driver diagnostic's source ID and location into root/include/graph navigation data.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     row (Int64): Zero-based displayed/schema/diagnostic row index.
#
# Returns:
#     false when stage < 0 || workspace.diagnostics[row * 4] <= 0. Otherwise returns the completed
#     action/predicate result.
fn ss_workspace_resolve_diagnostic(workspace :: SSWorkspace, row :: Int64) -> Bool {
    stage :: Int64 := workspace.diagnostic_stages[row];
    source_id :: Int64 := workspace.diagnostic_sources[row];
    workspace.diagnostic_targets[row] = 3;
    workspace.diagnostic_nodes[row] = -1;
    workspace.diagnostic_path_slots[row] = -1;
    if stage < 0 || workspace.diagnostics[row * 4] <= 0 {
        return false;
    }
    graph_source :: Bool := stage == 0
        && workspace.graph.source[0] > 0
        && ss_hash(workspace.graph.source) == ss_hash(workspace.document.source);
    if graph_source {
        # The graph engine emits stable node source IDs for declarations and body.
        node :: Int64 := source_id;
        if node > 0 && ss_graph_slot(workspace.graph, node) >= 0 {
            workspace.diagnostic_targets[row] = 2;
            workspace.diagnostic_nodes[row] = node;
            return true;
        }
    }
    if source_id == 0 {
        workspace.diagnostic_targets[row] = 0;
        return true;
    }
    if source_id > 0
        && source_id <= workspace.state[50]
        && workspace.state[51] == ss_hash(workspace.document.source)
        && workspace.state[52] == ss_hash(workspace.document.vertex) {
        workspace.diagnostic_targets[row] = 1;
        workspace.diagnostic_path_slots[row] = source_id - 1;
        return true;
    }
    return false;
}

# Map a numeric GLSL source ID to its cached include or graph node context.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_diagnostic_source_map(workspace :: SSWorkspace) -> Bool {
    workspace.state[50] = 0;
    workspace.state[51] = ss_hash(workspace.document.source);
    workspace.state[52] = ss_hash(workspace.document.vertex);
    if workspace.state[9] == 0 {
        i :: Int64 := 0;
        while i < workspace.document.state[10] {
            path :: Array<Int64> := workspace.document.dependencies[i];
            j :: Int64 := 1;
            while j <= path[0] {
                workspace.diagnostic_paths[i * 2048 + j - 1] = path[j];
                j = j + 1;
            }
            workspace.diagnostic_path_lengths[i] = path[0];
            i = i + 1;
        }
        workspace.state[50] = workspace.document.state[10];
        return true;
    }
    # Reuse the engine preprocessor's stable include IDs for the attempted build,
    # including failures; publication dependencies refer only to successful builds.
    @bounded<67108864> region shader_ui_source_map {
        prep :: SSPreprocessor := ss_preprocessor();
        vp :: String := se_text_read(workspace.document.vertex_path);
        fp :: String := se_text_read(workspace.document.fragment_path);
        if vp == "" {
            vp = se_text_read(workspace.document.path);
        }
        if fp == "" {
            fp = se_text_read(workspace.document.path);
        }
        try {
            if workspace.document.vertex[0] > 0 {
                ss_expand(prep, se_text_read(workspace.document.vertex), vp, 0, 0);
            }
            ss_expand(prep, se_text_read(workspace.document.source), fp, 0, 0);
        } catch (error) {
            # Partial include IDs remain useful for preprocessing diagnostics.
        }
        i :: Int64 := 0;
        while i < prep.state[0] {
            path :: Array<Int64> := prep.paths[i];
            j :: Int64 := 1;
            while j <= path[0] {
                workspace.diagnostic_paths[i * 2048 + j - 1] = path[j];
                j = j + 1;
            }
            workspace.diagnostic_path_lengths[i] = path[0];
            i = i + 1;
        }
        workspace.state[50] = prep.state[0];
    };
    return true;
}

# Load bounded cached include text for a diagnostic without replacing editable root source.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     row (Int64): Zero-based displayed/schema/diagnostic row index.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     false when path.size() == 0 || path.size() > 2048. Otherwise returns the completed
#     action/predicate result.
fn ss_workspace_diagnostic_include(workspace :: SSWorkspace, row :: Int64, path :: String) -> Bool {
    if path.size() == 0 || path.size() > 2048 {
        return false;
    }
    slot :: Int64 := 0;
    while slot < workspace.state[50] {
        if ss_workspace_diagnostic_path(workspace, slot) == path {
            break;
        }
        slot = slot + 1;
    }
    if slot == 32 {
        workspace.state[11] = 1;
        return false;
    }
    if slot == workspace.state[50] {
        i :: Int64 := 0;
        while i < path.size() {
            workspace.diagnostic_paths[slot * 2048 + i] = path.byte_at(i);
            i = i + 1;
        }
        workspace.diagnostic_path_lengths[slot] = path.size();
        workspace.state[50] = slot + 1;
    }
    workspace.diagnostic_targets[row] = 1;
    workspace.diagnostic_path_slots[row] = slot;
    return true;
}

# Resolve the retained resource path associated with a diagnostic row.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_workspace_diagnostic_path(workspace :: SSWorkspace, slot :: Int64) -> String {
    if slot < 0 || slot >= workspace.state[50] {
        return "";
    }
    return se_edit_slice(
        workspace.diagnostic_paths,
        slot * 2048 - 1,
        slot * 2048 + workspace.diagnostic_path_lengths[slot] - 1
    );
}

# Navigate to a root source location, cached include context, or graph node selected by a diagnostic.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     row (Int64): Zero-based displayed/schema/diagnostic row index.
#
# Returns:
#     false when row < 0 || row >= workspace.state[10]. Otherwise returns the completed
#     action/predicate result.
fn ss_workspace_diagnostic_jump(workspace :: SSWorkspace, row :: Int64) -> Bool {
    if row < 0 || row >= workspace.state[10] {
        return false;
    }
    kind :: Int64 := workspace.diagnostic_targets[row];
    line :: Int64 := workspace.diagnostics[row * 4];
    column :: Int64 := workspace.diagnostics[row * 4 + 1];
    if kind == 0 {
        workspace.layout.state[1] = workspace.diagnostic_stages[row];
        workspace.layout.state[6] = 1;
        return ss_code_jump(ss_workspace_editor(workspace), line, column);
    }
    if kind == 1 {
        path :: String := ss_workspace_diagnostic_path(workspace, workspace.diagnostic_path_slots[row]);
        return ss_workspace_include_view(workspace, path, line, column);
    }
    if kind == 2 {
        slot :: Int64 := ss_graph_slot(workspace.graph, workspace.diagnostic_nodes[row]);
        if slot < 0 {
            return false;
        }
        workspace.layout.state[1] = 2;
        workspace.layout.state[6] = 1;
        workspace.state[21] = slot;
        workspace.values[21] = vmath.max(
            0.0,
            ss_scale(workspace.graph.values[slot * 6 + 4]) - ss_scale(24.0)
        );
        workspace.values[22] = vmath.max(
            0.0,
            ss_scale(workspace.graph.values[slot * 6 + 5]) - ss_scale(90.0)
        );
        se_text_write(
            workspace.diagnostic_notice,
            "Graph node #"
                +string(workspace.diagnostic_nodes[row])
                +" / line "
                +string(line)
                +", column "
                +string(column)
        );
        return true;
    }
    se_text_write(
        workspace.diagnostic_notice,
        "Source ID "
            +string(workspace.diagnostic_sources[row])
            +" / line "
            +string(line)
            +", column "
            +string(column)
            +" has no source-view mapping; see build log"
    );
    return false;
}

# Consume a pending diagnostic-open request once for host path navigation.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     path (Array): File or resource path; relative-path handling follows the operation described
#         above.
#
# Returns:
#     false when workspace.panel.state[10] == 0. Otherwise returns the completed action/predicate
#     result.
fn ss_workspace_take_diagnostic_open(workspace :: SSWorkspace, path :: Array) -> Bool {
    if workspace.panel.state[10] == 0 {
        return false;
    }
    se_text_copy(path, workspace.diagnostic_open_path);
    workspace.panel.state[10] = 0;
    return true;
}

# Compile current root/graph source through the common last-good candidate path and refresh
# reflection/diagnostics.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The Boolean result of success.
fn ss_workspace_compile(app :: vraylib.App, workspace :: SSWorkspace) -> Bool {
    if workspace.document.state[0] != workspace.state[16] {
        ss_workspace_sync_source(workspace);
    }
    success :: Bool := ss_compile(app, workspace.document);
    workspace.state[9] = int64(!success);
    ss_workspace_diagnostic_source_map(workspace);
    ss_workspace_diagnostics(workspace, workspace.document.log);
    if success {
        workspace.state[28] = 0;
        workspace.state[29] = 0;
        ss_material_migrate(workspace.material, workspace.document);
        rows :: Array := ss_program(workspace.document).uniforms();
        i :: Int64 := 0;
        ss_panel_reflect(workspace.panel, workspace.document, rows);
        while i < rows.size() {
            row :: Array := rows[i];
            represented :: Bool := false;
            j :: Int64 := 0;
            while j < workspace.document.parameters.state[0] {
                if se_text_read(workspace.document.parameters.names[j]) == row[0] {
                    represented = true;
                }
                j = j + 1;
            }
            if !represented {
                if row[0] == "time" && int64(row[1]) == 5126 {
                    workspace.state[28] = 1;
                }
                if row[0] == "resolution" && int64(row[1]) == 35664 {
                    workspace.state[29] = 1;
                }
            }
            i = i + 1;
        }
    }
    return success;
}

# Save standalone workspace source/graph/pipeline state or queue the corresponding hosted save action.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
#
# Errors:
#     Source save failed.
fn ss_workspace_save(workspace :: SSWorkspace) -> Bool {
    if workspace.state[42] != 0 {
        workspace.state[43] = 1;
        return true;
    }
    try {
        if !ss_document_save(workspace.document, se_text_read(workspace.document.path)) {
            throw "Source save failed";
        }
        se_text_copy(workspace.saved_source, workspace.document.source);
        se_text_copy(workspace.saved_vertex, workspace.document.vertex);
        if !ss_workspace_save_layout(workspace) {
            return false;
        }
        workspace.state[33] = 0;
        return true;
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        workspace.state[33] = 1;
        return false;
    }
}

# Restore saved source/vertex/parameter state, coordinating hosted material reset when attached.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_reset(workspace :: SSWorkspace) -> Bool {
    if workspace.state[42] != 0 {
        workspace.state[46] = 1;
        return true;
    }
    ss_value_history_clear(workspace.panel);
    se_text_copy(workspace.document.source, workspace.saved_source);
    se_text_copy(workspace.document.vertex, workspace.saved_vertex);
    ss_code_reset_history(workspace.fragment);
    ss_code_reset_history(workspace.vertex);
    ss_document_source_change(workspace.document);
    p :: SSParameters := workspace.document.parameters;
    i :: Int64 := 0;
    while i < p.state[0] {
        if p.meta[i * 8 + 1] != 35678 {
            j :: Int64 := 0;
            while j < p.meta[i * 8 + 3] {
                ss_workspace_parameter_set(workspace, p.meta[i * 8], j, p.defaults[i * 64 + j]);
                j = j + 1;
            }
        }
        i = i + 1;
    }
    workspace.state[9] = 0;
    return true;
}

# Host supplies a borrowed selected model. Never closed or assigned persistently.

# Borrow a selected scene model for the material preview without taking ownership of its geometry.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     model (vraylib.Model): Managed model whose geometry is borrowed for this draw.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_selected(workspace :: SSWorkspace, model :: vraylib.Model) -> Bool {
    if model.session != workspace.state[0] || !model.valid() {
        workspace.state[8] = 0;
        return false;
    }
    workspace.state[8] = model.token;
    return true;
}

# Validate/queue Apply for the current successful source/material state.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     false when !ss_program(workspace.document).valid() ||
#     ss_document_compile_dirty(workspace.document). Otherwise returns the completed action/predicate
#     result.
fn ss_workspace_apply(workspace :: SSWorkspace) -> Bool {
    if !ss_program(workspace.document).valid() || ss_document_compile_dirty(workspace.document) {
        return false;
    }
    ss_material_apply(workspace.document, workspace.material);
    workspace.state[17] = 1;
    return true;
}

# Consume a queued Apply request exactly once.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The Boolean result of requested.
fn ss_workspace_take_apply(workspace :: SSWorkspace) -> Bool {
    requested :: Bool := workspace.state[17] != 0;
    workspace.state[17] = 0;
    return requested;
}

# Claim whole-frame workspace input ownership so host and scene shortcuts cannot also dispatch.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_claim_input(workspace :: SSWorkspace) -> Bool {
    workspace.panel.state[14] = 1;
    workspace.state[55] = 1;
    return true;
}

# Complete the host-coordinated reset and update saved/focus/action state according to success.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_reset_complete(workspace :: SSWorkspace) -> Bool {
    workspace.state[46] = 0;
    workspace.state[55] = 1;
    workspace.parameter_editor.state[1] = -1;
    workspace.panel.draft[0] = 0;
    ss_value_history_clear(workspace.panel);
    ss_code_reset_history(workspace.fragment);
    ss_code_reset_history(workspace.vertex);
    return true;
}

# Process source/graph/parameter/preview keyboard and mouse input through workspace modal gates.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     bounds (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#     dt (Float64): Frame delta time in seconds.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Shader workspace belongs to another app.
fn ss_workspace_update(
    app :: vraylib.App,
    workspace :: SSWorkspace,
    bounds :: vraylib.Rectangle,
    dt :: Float64
) -> Bool {
    if app.token != workspace.state[0] {
        throw "Shader workspace belongs to another app";
    }
    claimed :: Bool := workspace.panel.state[14] != 0;
    workspace.panel.state[14] = 0;
    workspace.state[55] = int64(claimed || workspace.parameter_editor.state[1] >= 0 || workspace.panel.draft[0] != 0 || workspace.panel.state[2] != 0 || workspace.panel.history_state[0] != 0);
    if workspace.panel.history_state[0] != 0 && !vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) {
        ss_value_end(workspace.panel, workspace.document, workspace.material);
    }
    workspace.panel.state[9] = int64(workspace.parameter_editor.state[1] >= 0);
    if workspace.state[55] == 0 {
        ss_layout_update(workspace.layout, bounds);
    } else {
        ss_theme_dpi(workspace.layout.values[3]);
    }
    workspace.values[1] = vmath.clamp(dt, 0.0, 0.1);
    if workspace.layout.state[6] != 0
        && workspace.layout.state[5] == 0
        && vraylib.get_time() >= workspace.values[23] {
        ss_workspace_save_layout(workspace);
    }
    if workspace.state[14] == 0 {
        workspace.values[0] = workspace.values[0] + workspace.values[1];
    }
    if workspace.document.state[0] != workspace.state[16] {
        ss_workspace_sync_source(workspace);
    }
    if !claimed && workspace.parameter_editor.state[1] >= 0 {
        control_paste :: Bool := (vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL) || vraylib.is_key_down(vraylib.KEY_RIGHT_CONTROL))
            && vraylib.is_key_pressed(vraylib.KEY_V);
        accepted :: Bool := true;
        if control_paste {
            accepted = ss_code_string_valid(vraylib.get_clipboard_text());
        }
        if !accepted {
            se_text_write(workspace.log, "Invalid UTF-8 clipboard bytes; field unchanged");
            ss_workspace_diagnostics(workspace, workspace.log);
        } else {
            if se_edit_input(workspace.parameter_editor) {
                ss_workspace_commit_field(workspace);
            }
        }
        if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) && workspace.panel.state[2] == 1 {
            workspace.panel.state[2] = 0;
        }
    } else {
        if workspace.state[55] == 0
            && workspace.layout.state[4] == 0
            && workspace.layout.state[1] < 2
            && workspace.layout.state[5] == 0 {
            if ss_code_input(ss_workspace_editor(workspace), ss_workspace_source_box(workspace, bounds)) {
                ss_document_source_change(workspace.document);
                workspace.state[9] = 0;
                workspace.state[16] = workspace.document.state[0];
                workspace.state[36] = ss_hash(workspace.document.source);
                workspace.state[37] = ss_hash(workspace.document.vertex);
            }
        }
    }
    control :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_CONTROL)
        || vraylib.is_key_down(vraylib.KEY_RIGHT_CONTROL);
    shift :: Bool := vraylib.is_key_down(vraylib.KEY_LEFT_SHIFT)
        || vraylib.is_key_down(vraylib.KEY_RIGHT_SHIFT);
    if control && vraylib.is_key_pressed(vraylib.KEY_S) && workspace.state[55] == 0 {
        if shift {
            ss_workspace_save_as_begin(workspace);
        } else {
            ss_workspace_save(workspace);
        }
    }
    if control && vraylib.is_key_pressed(vraylib.KEY_ENTER) && workspace.state[55] == 0 {
        ss_workspace_compile(app, workspace);
    }
    if control
        && workspace.state[55] == 0
        && workspace.fragment.state[4] == 0
        && workspace.vertex.state[4] == 0 {
        if vraylib.is_key_pressed(vraylib.KEY_Z) {
            ss_workspace_parameter_undo(workspace, shift);
        }
        if vraylib.is_key_pressed(vraylib.KEY_Y) {
            ss_workspace_parameter_undo(workspace, true);
        }
    }
    preview :: vraylib.Rectangle := ss_layout_region(workspace.layout, bounds, 3);
    if workspace.state[55] == 0
        && se_hover(preview)
        && vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_RIGHT) {
        motion :: vraylib.Vector2 := vraylib.get_mouse_delta();
        workspace.values[18] = workspace.values[18] + motion.x * 0.008;
        workspace.values[19] = vmath.clamp(workspace.values[19] - motion.y * 0.008, -1.3, 1.3);
    }
    if workspace.state[55] == 0 && se_hover(preview) {
        workspace.values[20] = vmath.clamp(
            workspace.values[20] - vraylib.get_mouse_wheel_move() * 0.3,
            1.5,
            12.0
        );
    }
    if !claimed
        && workspace.panel.draft[0] != 0
        && workspace.panel.state[9] == 0
        && vraylib.is_key_pressed(vraylib.KEY_ESCAPE) {
        workspace.panel.draft[0] = 0;
    }
    return true;
}

# Synchronize source editor indexing/history/reset buffers after loading or generating shader source.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_sync_source(workspace :: SSWorkspace) -> Bool {
    fragment_hash :: Int64 := ss_hash(workspace.document.source);
    vertex_hash :: Int64 := ss_hash(workspace.document.vertex);
    if fragment_hash != workspace.state[36] {
        ss_code_reset_history(workspace.fragment);
        ss_value_history_clear(workspace.panel);
        workspace.state[36] = fragment_hash;
    }
    if vertex_hash != workspace.state[37] {
        ss_code_reset_history(workspace.vertex);
        ss_value_history_clear(workspace.panel);
        workspace.state[37] = vertex_hash;
    }
    workspace.state[16] = workspace.document.state[0];
    return true;
}

# Persist the workspace layout and report save success without silently clearing failures.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The Boolean result of success.
fn ss_workspace_save_layout(workspace :: SSWorkspace) -> Bool {
    success :: Bool := false;
    @bounded<1048576> region shader_ui_layout_save {
        try {
            success = ss_layout_save(workspace.layout, se_text_read(workspace.layout_path));
            if !success {
                se_text_write(
                    workspace.log,
                    "Layout save failed; pending retry: " + se_text_read(workspace.layout_path)
                );
            }
        } catch (error) {
            se_text_write(workspace.log, string(error));
        }
    };
    if !success {
        workspace.layout.state[6] = 1;
        workspace.state[33] = 1;
        workspace.values[23] = vraylib.get_time() + 1.0;
        ss_workspace_diagnostics(workspace, workspace.log);
    } else {
        workspace.values[23] = 0.0;
        workspace.state[33] = 0;
    }
    return success;
}

# Render the current last-good shader/material on the selected preview geometry or run the prepared
# Rendering pipeline.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_preview(app :: vraylib.App, workspace :: SSWorkspace) -> Bool {
    size :: Int64 := workspace.state[15];
    target :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[1], app.token);
    reference :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[2], app.token);
    difference :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[3], app.token);
    if target.width() != size {
        target.resize(size, size);
        reference.resize(size, size);
        difference.resize(size, size);
        workspace.state[26] = 0;
    }
    shader :: vraylib.Shader := ss_program(workspace.document);
    if workspace.state[56] == 0 {
        if shader.valid() {
            applied :: Int64 := workspace.document.state[3];
            applied_values :: Int64 := workspace.document.state[9];
            try {
                ss_material_apply(workspace.document, workspace.material);
            } finally {
                workspace.document.state[3] = applied;
                workspace.document.state[9] = applied_values;
            }
            if workspace.state[28] != 0 {
                shader.set("time", workspace.values[0]);
            }
            if workspace.state[29] != 0 {
                shader.set("resolution", vraylib.vec2(float64(size), float64(size)));
            }
        }
        camera :: vraylib.Camera := vraylib.Camera(
            workspace.state[4],
            app.token,
            0.0,
            0.0,
            vraylib.CAMERA_ORBITAL
        );
        yaw :: Float64 := workspace.values[18];
        pitch :: Float64 := workspace.values[19];
        distance :: Float64 := workspace.values[20];
        camera.set_view(
            vraylib.vec3(
                vmath.sin(yaw) * vmath.cos(pitch) * distance,
                vmath.sin(pitch) * distance,
                vmath.cos(yaw) * vmath.cos(pitch) * distance
            ),
            vraylib.vec3(0.0, 0.0, 0.0)
        );
        target.begin();
        try {
            app.clear(vraylib.rgba(28, 32, 39, 255));
            camera.begin();
            try {
                token :: Int64 := workspace.state[5 + workspace.state[35]];
                if workspace.state[35] == 3 {
                    token = workspace.state[8];
                }
                model :: vraylib.Model := vraylib.Model(token, app.token);
                if token != 0 && model.valid() {
                    if shader.valid() {
                        model.draw_material(vraylib.math_matrix_identity(), shader, 0, vraylib.WHITE);
                    } else {
                        model.draw(vraylib.vec3(0.0, 0.0, 0.0), 1.0, vraylib.WHITE);
                    }
                }
            } finally {
                camera.end();
            }
        } finally {
            target.end();
        }
    } else {
        target = vraylib.RenderTarget(workspace.state[56], app.token);
    }
    workspace.state[25] = target.token;
    workspace.pipeline.context[0] = workspace.values[0];
    workspace.pipeline.context[1] = app.dt();
    workspace.pipeline.context[2] = 1.0;
    if workspace.state[24] != 0 {
        @bounded<16777216> region shader_ui_pipeline {
            ss_pipeline_prepare(app, workspace.pipeline, target.width(), target.height());
            rendered :: vraylib.RenderTarget := ss_pipeline_run(
                app,
                workspace.pipeline,
                workspace.documents,
                workspace.document_ids,
                target
            );
            workspace.state[25] = int64(rendered.token);
        };
    }
    output :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[25], app.token);
    workspace.state[25] = output.token;
    dimensions :: Int64 := output.width() * 8192 + output.height();
    if workspace.state[26] != 0 && workspace.state[32] != dimensions {
        workspace.state[26] = 0;
    }
    if workspace.state[12] != 0 {
        ss_workspace_capture_reference(workspace, output);
    }
    if workspace.state[13] == 2 && workspace.state[26] != 0 {
        compare :: vraylib.Shader := vraylib.Shader(workspace.state[34], app.token);
        compare.set_texture("referenceTexture", reference.texture());
        difference.begin();
        try {
            app.clear(vraylib.BLACK);
            compare.begin();
            try {
                output.present(float64(size), float64(size));
            } finally {
                compare.end();
            }
        } finally {
            difference.end();
        }
    }
    if workspace.state[27] != 0 && workspace.state[26] != 0 {
        pixels :: Array<Float64> := output.read_pixels();
        squared :: Float64 := 0.0;
        peak :: Float64 := 0.0;
        i :: Int64 := 0;
        while i < pixels.size() {
            delta :: Float64 := vmath.abs(pixels[i] - workspace.reference_pixels[i]);
            squared = squared + delta * delta;
            peak = vmath.max(peak, delta);
            i = i + 1;
        }
        workspace.values[3] = peak;
        workspace.values[4] = vmath.sqrt(squared / float64(pixels.size()));
        x :: Int64 := int64(vmath.clamp(workspace.values[5], 0.0, float64(output.width() - 1)));
        y :: Int64 := int64(vmath.clamp(workspace.values[6], 0.0, float64(output.height() - 1)));
        index :: Int64 := (y * output.width() + x) * 4;
        i = 0;
        while i < 4 {
            workspace.values[7 + i] = pixels[index + i];
            workspace.values[11 + i] = workspace.reference_pixels[index + i];
            i = i + 1;
        }
        workspace.state[27] = 0;
    }
    return true;
}

# Copy matching linear preview pixels into the retained reference buffer for later difference
# inspection.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     output (vraylib.RenderTarget): Managed render target owned by the current application session.
#
# Returns:
#     True when the operation completes without throwing.
#
# Errors:
#     Reference source belongs to another application or is unavailable.
#     Reference capture exceeds 512 x 512 pixel capacity.
#     Exact reference target copy failed.
fn ss_workspace_capture_reference(workspace :: SSWorkspace, output :: vraylib.RenderTarget) -> Bool {
    if output.session != workspace.state[0] || !output.valid() {
        throw "Reference source belongs to another application or is unavailable";
    }
    workspace.state[12] = 0;
    if output.width() * output.height() * 4 > workspace.reference_pixels.size() {
        throw "Reference capture exceeds 512 x 512 pixel capacity";
    }
    reference :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[2], workspace.state[0]);
    workspace.state[26] = 0;
    reference.resize(output.width(), output.height());
    if !reference.copy_from(output) {
        throw "Exact reference target copy failed";
    }
    samples :: Array<Float64> := reference.read_pixels();
    i :: Int64 := 0;
    while i < samples.size() {
        workspace.reference_pixels[i] = samples[i];
        i = i + 1;
    }
    workspace.state[26] = 1;
    workspace.state[32] = output.width() * 8192 + output.height();
    workspace.values[2] = workspace.values[0];
    return true;
}

# Queue/dispatch a toolbar action while respecting hosted state and pending edits.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     label (String): Text displayed by the control.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#     enabled (Bool): Whether the control, pass, or feature is enabled.
#
# Returns:
#     The Boolean result of ss_theme_input[0] != 0 && enabled && se_hover(box) &&
#     vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT).
fn ss_workspace_action(
    workspace :: SSWorkspace,
    label :: String,
    box :: vraylib.Rectangle,
    enabled :: Bool
) -> Bool {
    vraylib.draw_rectangle_rec(box, ss_color(2));
    vraylib.draw_rectangle_lines_ex(box, ss_scale(1.0), ss_color(3));
    tint :: vraylib.Color := ss_color(5);
    if enabled {
        tint = ss_color(4);
    }
    se_draw_text(
        se_fit(label, box.width - ss_scale(12.0), ss_font_size()),
        int64(box.x + ss_scale(6.0)),
        int64(box.y + ss_scale(7.0)),
        ss_font_size(),
        tint
    );
    if ss_theme_input[0] != 0 && enabled && se_hover(box) {
        tip :: Int64 := 0;
        if label == "Compile" {
            tip = 1;
        }
        if label == "Save" {
            tip = 2;
        }
        if label == "Save As" {
            tip = 3;
        }
        if label == "Reload" {
            tip = 4;
        }
        if label == "Expose uniform" || label == "Schema" {
            tip = 5;
        }
        if label == "Apply schema" {
            tip = 6;
        }
        if label == "Undo value" || label == "Redo value" {
            tip = 7;
        }
        if label == "Open externally" {
            tip = 8;
        }
        if label == "Capture reference" {
            tip = 9;
        }
        workspace.panel.state[15] = tip;
        workspace.values[24] = box.x;
        workspace.values[25] = box.y + box.height;
    }
    return ss_theme_input[0] != 0
        && enabled
        && se_hover(box)
        && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT);
}

# Draw Shader Studio Compile/Apply/Save/Reset and source/graph navigation controls.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     bounds (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_toolbar(app :: vraylib.App, workspace :: SSWorkspace, bounds :: vraylib.Rectangle) -> Bool {
    box :: vraylib.Rectangle := ss_layout_region(workspace.layout, bounds, 1);
    vraylib.draw_rectangle_rec(box, ss_color(1));
    x :: Float64 := box.x + ss_scale(8.0);
    y :: Float64 := box.y + ss_scale(5.0);
    h :: Float64 := ss_scale(29.0);
    available :: Bool := workspace.panel.draft[0] == 0 && workspace.panel.state[2] == 0;
    if ss_workspace_action(workspace, "Compile", se_rect(x, y, ss_scale(80.0), h), available) {
        ss_workspace_compile(app, workspace);
    }
    if ss_workspace_action(workspace, "Save", se_rect(x + ss_scale(86.0), y, ss_scale(58.0), h), available) {
        if ss_workspace_commit_field(workspace) {
            ss_workspace_save(workspace);
        }
    }
    if ss_workspace_action(
        workspace,
        "Reset",
        se_rect(x + ss_scale(150.0), y, ss_scale(58.0), h),
        available
    ) {
        ss_workspace_reset(workspace);
    }
    if ss_workspace_action(
        workspace,
        "Apply",
        se_rect(x + ss_scale(214.0), y, ss_scale(58.0), h),
        available && !ss_document_compile_dirty(workspace.document) && ss_program(workspace.document).valid()
    ) {
        ss_workspace_apply(workspace);
    }
    se_draw_heading(
        ss_workspace_status(workspace),
        int64(x + ss_scale(290.0)),
        int64(y + ss_scale(7.0)),
        ss_font_size(),
        ss_color(6)
    );
    if box.width > ss_scale(760.0) {
        if ss_workspace_action(
            workspace,
            "Save As",
            se_rect(x + ss_scale(400.0), y, ss_scale(86.0), h),
            available
        ) {
            ss_workspace_save_as_begin(workspace);
        }
        if ss_workspace_action(
            workspace,
            "Reload",
            se_rect(x + ss_scale(492.0), y, ss_scale(78.0), h),
            available
        ) {
            ss_workspace_reload(workspace, false);
        }
        se_draw_text(
            se_fit(se_text_read(workspace.document.path), box.width - ss_scale(600.0), ss_font_size()),
            int64(x + ss_scale(584.0)),
            int64(y + ss_scale(7.0)),
            ss_font_size(),
            ss_color(5)
        );
    }
    return true;
}

# Draw reflected/exposed parameter controls with typed values, orphan status, and schema-edit actions.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_parameters(workspace :: SSWorkspace, box :: vraylib.Rectangle) -> Bool {
    if ss_tab(
        "Values",
        se_rect(box.x, box.y + ss_scale(32.0), box.width * 0.5, ss_scale(30.0)),
        workspace.panel.state[12] == 0
    ) && workspace.panel.draft[0] == 0 {
        workspace.panel.state[12] = 0;
    }
    if ss_tab(
        "Expose / Schema",
        se_rect(box.x + box.width * 0.5, box.y + ss_scale(32.0), box.width * 0.5, ss_scale(30.0)),
        workspace.panel.state[12] != 0
    ) && workspace.panel.draft[0] == 0 {
        workspace.panel.state[12] = 1;
    }
    if workspace.panel.draft[0] != 0 {
        return ss_workspace_schema_form(workspace, box);
    }
    if workspace.panel.state[12] == 1 {
        return ss_workspace_uniform_list(workspace, box);
    }
    p :: SSParameters := workspace.document.parameters;
    y :: Float64 := box.y + ss_scale(104.0) - float64(workspace.state[18]) * ss_scale(28.0);
    if ss_workspace_action(
        workspace,
        "Undo value",
        se_rect(
            box.x + ss_scale(8.0),
            box.y + ss_scale(68.0),
            (box.width - ss_scale(20.0)) * 0.5,
            ss_scale(26.0)
        ),
        workspace.panel.history_state[5] > 0
    ) {
        ss_workspace_parameter_undo(workspace, false);
    }
    if ss_workspace_action(
        workspace,
        "Redo value",
        se_rect(
            box.x + box.width * 0.5,
            box.y + ss_scale(68.0),
            (box.width - ss_scale(20.0)) * 0.5,
            ss_scale(26.0)
        ),
        workspace.panel.history_state[5] < workspace.panel.history_state[6]
    ) {
        ss_workspace_parameter_undo(workspace, true);
    }
    if ss_theme_input[0] != 0 && se_hover(box) {
        workspace.state[18] = int64(vmath.max(
            0.0,
            float64(workspace.state[18]) - vraylib.get_mouse_wheel_move() * 3.0
        ));
    }
    i :: Int64 := 0;
    while i < p.state[0] {
        if y >= box.y + ss_scale(100.0) {
            se_draw_heading(
                se_fit(
                    se_text_read(p.names[i]) + " / " + se_text_read(p.groups[i]),
                    box.width - ss_scale(16.0),
                    ss_font_size()
                ),
                int64(box.x + ss_scale(8.0)),
                int64(y),
                ss_font_size(),
                ss_color(4)
            );
        }
        y = y + ss_scale(22.0);
        status :: String := "Uncompiled";
        if p.meta[i * 8 + 6] == 1 {
            status = "Active";
        }
        if p.meta[i * 8 + 6] == 2 {
            status = "Optimized out / missing";
        }
        if p.meta[i * 8 + 6] == 3 {
            status = "Type mismatch";
        }
        supported :: Bool := ss_upload_supported(p.meta[i * 8 + 1])
            && !(p.meta[i * 8 + 1] == 35678 && p.meta[i * 8 + 2] > 1);
        if !supported {
            status = "Unsupported upload / sampler array";
        }
        if p.meta[i * 8 + 6] == 1 && p.meta[i * 8 + 7] < 0 {
            status = "Unsupported block / negative location";
            supported = false;
        }
        if y >= box.y + ss_scale(100.0) {
            se_draw_text(
                status + "  GL " + string(p.meta[i * 8 + 1]) + " x" + string(p.meta[i * 8 + 2]),
                int64(box.x + ss_scale(8.0)),
                int64(y),
                int64(ss_scale(12.0)),
                ss_color(5)
            );
        }
        y = y + ss_scale(22.0);
        j :: Int64 := 0;
        while j < p.meta[i * 8 + 3] {
            field :: vraylib.Rectangle := se_rect(
                box.x + ss_scale(8.0),
                y,
                vmath.max(0.0, box.width - ss_scale(16.0)),
                ss_scale(26.0)
            );
            value :: Float64 := p.values[i * 64 + j];
            low :: Float64 := p.ranges[i * 2];
            high :: Float64 := p.ranges[i * 2 + 1];
            if y >= box.y + ss_scale(100.0) && y < box.y + box.height {
                vraylib.draw_rectangle_rec(field, ss_color(0));
                if high > low {
                    vraylib.draw_rectangle_rec(
                        se_rect(field.x, field.y, field.width * (value - low) / (high - low), field.height),
                        vraylib.rgba(40, 62, 79, 255)
                    );
                }
                focused :: Bool := workspace.parameter_editor.state[1] == 0
                    && workspace.state[38] == i
                    && workspace.state[39] == j;
                if focused {
                    se_field(
                        "[" + string(j) + "]",
                        se_text_read(workspace.parameter_editor.text),
                        field,
                        true,
                        true
                    );
                } else {
                    ss_mono(
                        "[" + string(j) + "] " + string(value) + " " + se_text_read(p.units[i]),
                        field.x + ss_scale(6.0),
                        field.y + ss_scale(5.0),
                        ss_color(4)
                    );
                }
                if ss_theme_input[0] != 0
                    && supported
                    && se_hover(field)
                    && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_RIGHT)
                    && p.meta[i * 8 + 1] != 35678 {
                    ss_workspace_value_field_begin(workspace, p.meta[i * 8], j);
                }
                if ss_theme_input[0] != 0
                    && supported
                    && se_hover(field)
                    && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT)
                    && p.meta[i * 8 + 1] != 35678 {
                    workspace.state[19] = i;
                    workspace.state[20] = j;
                    ss_value_begin(workspace.panel, workspace.document, workspace.material, p.meta[i * 8]);
                }
                if ss_theme_input[0] != 0
                    && !focused
                    && workspace.state[19] == i
                    && workspace.state[20] == j
                    && vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) {
                    mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
                    next :: Float64 := low + (high - low) * vmath.clamp(
                        (mouse.x - field.x) / vmath.max(1.0, field.width),
                        0.0,
                        1.0
                    );
                    if !ss_continuous(p.meta[i * 8 + 1]) {
                        next = vmath.floor(next + 0.5);
                    }
                    ss_workspace_parameter_edit(workspace, p.meta[i * 8], j, next);
                }
            }
            y = y + ss_scale(28.0);
            j = j + 1;
        }
        if y >= box.y + ss_scale(100.0) {
            half :: Float64 := vmath.max(0.0, (box.width - ss_scale(20.0)) * 0.5);
            if ss_workspace_action(
                workspace,
                "Reset",
                se_rect(box.x + ss_scale(8.0), y, half, ss_scale(26.0)),
                supported && p.meta[i * 8 + 1] != 35678
            ) {
                ss_workspace_parameter_reset(workspace, p.meta[i * 8]);
            }
            if ss_workspace_action(
                workspace,
                "Schema",
                se_rect(box.x + box.width * 0.5, y, half, ss_scale(26.0)),
                true
            ) {
                ss_workspace_schema_begin(workspace, p.meta[i * 8]);
            }
        }
        y = y + ss_scale(42.0);
        i = i + 1;
    }
    if !vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) {
        ss_value_end(workspace.panel, workspace.document, workspace.material);
        workspace.state[19] = -1;
    }
    return true;
}

# Resolve the instance override compatible with a displayed parameter's published schema.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     id (Int64): Stable identifier or generation-checked scene handle, according to the containing
#         API.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     The Boolean result of ss_workspace_parameter_set(workspace, id, component, value).
fn ss_workspace_parameter_override(
    workspace :: SSWorkspace,
    id :: Int64,
    component :: Int64,
    value :: Float64
) -> Bool {
    return ss_workspace_parameter_set(workspace, id, component, value);
}

# Validate and publish a numeric parameter edit to coupled document/material state without recompiling
# source.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     id (Int64): Stable identifier or generation-checked scene handle, according to the containing
#         API.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     The Boolean result of ss_material_parameter_set(workspace.document, workspace.material, id,
#     component, value).
#
# Errors:
#     Unknown shader parameter.
#     Sampler parameters require a texture binding.
fn ss_workspace_parameter_set(
    workspace :: SSWorkspace,
    id :: Int64,
    component :: Int64,
    value :: Float64
) -> Bool {
    p :: SSParameters := workspace.document.parameters;
    slot :: Int64 := ss_parameter_slot(p, id);
    if slot < 0 {
        throw "Unknown shader parameter";
    }
    if p.meta[slot * 8 + 1] == 35678 {
        throw "Sampler parameters require a texture binding";
    }
    # Engine stages/validates compatible (ID,type,arraylen) migration and both writes.
    return ss_material_parameter_set(workspace.document, workspace.material, id, component, value);
}

# Begin or update a parameter drag/precise-entry interaction and its retained before-image.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     id (Int64): Stable identifier or generation-checked scene handle, according to the containing
#         API.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_parameter_edit(
    workspace :: SSWorkspace,
    id :: Int64,
    component :: Int64,
    value :: Float64
) -> Bool {
    try {
        ss_value_begin(workspace.panel, workspace.document, workspace.material, id);
        changed :: Bool := ss_workspace_parameter_set(workspace, id, component, value);
        if workspace.state[19] < 0 {
            ss_value_end(workspace.panel, workspace.document, workspace.material);
        }
        return changed;
    } catch (error) {
        workspace.state[19] = -1;
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Restore an available parameter-value undo/redo snapshot.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     redo (Bool): True restores the next snapshot; false restores the previous snapshot.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_parameter_undo(workspace :: SSWorkspace, redo :: Bool) -> Bool {
    try {
        return ss_value_undo(workspace.panel, workspace.document, workspace.material, redo);
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Restore parameter values to the saved/default reset state through validated instance updates.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     id (Int64): Stable identifier or generation-checked scene handle, according to the containing
#         API.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
#
# Errors:
#     Unknown reset parameter.
fn ss_workspace_parameter_reset(workspace :: SSWorkspace, id :: Int64) -> Bool {
    try {
        slot :: Int64 := ss_parameter_slot(workspace.document.parameters, id);
        if slot < 0 {
            throw "Unknown reset parameter";
        }
        ss_value_begin(workspace.panel, workspace.document, workspace.material, id);
        i :: Int64 := 0;
        while i < workspace.document.parameters.meta[slot * 8 + 3] {
            ss_workspace_parameter_set(
                workspace,
                id,
                i,
                workspace.document.parameters.defaults[slot * 64 + i]
            );
            i = i + 1;
        }
        return ss_value_end(workspace.panel, workspace.document, workspace.material);
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Report whether pending exact entry, schema, path, source, or workspace modal state owns input.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The Boolean result of workspace.state[55] != 0 || workspace.parameter_editor.state[1] >= 0 ||
#     workspace.panel.draft[0] != 0 || workspace.panel.state[2] != 0.
fn ss_workspace_input_captured(workspace :: SSWorkspace) -> Bool {
    return workspace.state[55] != 0
        || workspace.parameter_editor.state[1] >= 0
        || workspace.panel.draft[0] != 0
        || workspace.panel.state[2] != 0;
}

# Resolve the baseline numeric value used by a precise-entry field.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     field (Int64): Zero-based numeric property/component or UI field identifier.
#     id (Int64): Stable identifier or generation-checked scene handle, according to the containing
#         API.
#     component (Int64): Zero-based component within the selected parameter row.
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_number_baseline(
    workspace :: SSWorkspace,
    field :: Int64,
    id :: Int64,
    component :: Int64,
    value :: Float64
) -> Bool {
    workspace.panel.state[17] = ss_hash(workspace.parameter_editor.text);
    workspace.panel.state[18] = field;
    workspace.panel.state[19] = id;
    workspace.panel.state[21] = component;
    workspace.panel.state[22] = workspace.document.state[0];
    workspace.values[27] = value;
    return true;
}

# Parse a numeric field according to its active parameter/schema component type.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     field (Int64): Zero-based numeric property/component or UI field identifier.
#     id (Int64): Stable identifier or generation-checked scene handle, according to the containing
#         API.
#     component (Int64): Zero-based component within the selected parameter row.
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#
# Returns:
#     The Float64 value selected by the conditions described above.
fn ss_workspace_field_number(
    workspace :: SSWorkspace,
    field :: Int64,
    id :: Int64,
    component :: Int64,
    text :: String
) -> Float64 {
    # Runtime String(Float64) displays six significant digits. An untouched field
    # preserves the original Float64 rather than committing its rounded display.
    if workspace.panel.state[18] == field
        && workspace.panel.state[19] == id
        && workspace.panel.state[21] == component
        && workspace.panel.state[22] == workspace.document.state[0]
        && workspace.panel.state[17] == ss_hash(workspace.parameter_editor.text) {
        return workspace.values[27];
    }
    return ss_number(text);
}

# Open exact numeric entry for a selected parameter component with its current value.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     id (Int64): Stable identifier or generation-checked scene handle, according to the containing
#         API.
#     component (Int64): Zero-based component within the selected parameter row.
#
# Returns:
#     false when slot < 0 || component < 0; false when component >=
#     workspace.document.parameters.meta[slot * 8 + 3]. Otherwise returns the completed
#     action/predicate result.
fn ss_workspace_value_field_begin(workspace :: SSWorkspace, id :: Int64, component :: Int64) -> Bool {
    slot :: Int64 := ss_parameter_slot(workspace.document.parameters, id);
    if slot < 0 || component < 0 {
        return false;
    }
    if component >= workspace.document.parameters.meta[slot * 8 + 3] {
        return false;
    }
    value :: Float64 := workspace.document.parameters.values[slot * 64 + component];
    workspace.state[38] = slot;
    workspace.state[39] = component;
    workspace.state[44] = workspace.document.parameters.meta[slot * 8 + 1];
    workspace.state[45] = workspace.document.parameters.meta[slot * 8 + 2];
    se_edit_begin(workspace.parameter_editor, 0, id, string(value));
    ss_workspace_number_baseline(workspace, 0, id, component, value);
    workspace.state[55] = 1;
    workspace.fragment.state[4] = 0;
    workspace.vertex.state[4] = 0;
    return true;
}

# Field dispatch shared with the host. 0 = exact value; 100/101 range,
# 102/103 group/units; 200..263 defaults; 900 = Save As path.

# Validate and commit the focused numeric/range/group/units/default/path field, retaining invalid
# input for correction.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     true when field < 0. Otherwise returns the completed action/predicate result.
#
# Errors:
#     Parameter shape changed; reopen the field.
#     Save As requires a valid UTF-8 path of at most 2048 bytes.
#     Schema draft is no longer open.
#     Unknown shader UI field ID.
fn ss_workspace_commit_field(workspace :: SSWorkspace) -> Bool {
    field :: Int64 := workspace.parameter_editor.state[1];
    if field < 0 {
        return true;
    }
    try {
        text :: String := se_text_read(workspace.parameter_editor.text);
        if field == 0 {
            p :: SSParameters := workspace.document.parameters;
            id :: Int64 := workspace.parameter_editor.state[14];
            slot :: Int64 := ss_parameter_slot(p, id);
            if slot < 0
                || p.meta[slot * 8 + 1] != workspace.state[44]
                || p.meta[slot * 8 + 2] != workspace.state[45] {
                throw "Parameter shape changed; reopen the field";
            }
            ss_value_begin(workspace.panel, workspace.document, workspace.material, id);
            ss_workspace_parameter_set(
                workspace,
                id,
                workspace.state[39],
                ss_workspace_field_number(workspace, field, id, workspace.state[39], text)
            );
            ss_value_end(workspace.panel, workspace.document, workspace.material);
        } else {
            if field == 900 {
                if text.size() == 0 || text.size() > 2048 || !ss_code_string_valid(text) {
                    throw "Save As requires a valid UTF-8 path of at most 2048 bytes";
                }
                workspace.parameter_editor.state[1] = -1;
                return ss_workspace_save_as_request(workspace, text, false);
            } else {
                if workspace.panel.draft[0] == 0 {
                    throw "Schema draft is no longer open";
                }
                if field == 100 || field == 101 {
                    workspace.panel.numbers[field - 100] = ss_workspace_field_number(
                        workspace,
                        field,
                        workspace.panel.draft[1],
                        field - 100,
                        text
                    );
                } else {
                    if field == 102 {
                        se_text_write(workspace.panel.group_name, text);
                    } else {
                        if field == 103 {
                            se_text_write(workspace.panel.units, text);
                        } else {
                            if field >= 200 && field < 200 + workspace.panel.draft[4] {
                                component :: Int64 := field - 200;
                                value :: Float64 := ss_workspace_field_number(
                                    workspace,
                                    field,
                                    workspace.panel.draft[1],
                                    component,
                                    text
                                );
                                workspace.panel.numbers[component + 2] = value;
                                if workspace.panel.draft[7] == 0 {
                                    workspace.panel.numbers[component + 66] = value;
                                }
                            } else {
                                throw "Unknown shader UI field ID";
                            }
                        }
                    }
                }
            }
        }
        workspace.parameter_editor.state[1] = -1;
        return true;
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Find an unused positive stable schema parameter ID for uniform exposure.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The computed Int64 value: high + 1.
#
# Errors:
#     Stable parameter ID capacity exhausted.
fn ss_workspace_new_parameter_id(workspace :: SSWorkspace) -> Int64 {
    high :: Int64 := 0;
    i :: Int64 := 0;
    while i < workspace.document.parameters.state[0] {
        high = int64(vmath.max(float64(high), float64(workspace.document.parameters.meta[i * 8])));
        i = i + 1;
    }
    i = 0;
    while i < workspace.material.state[2] {
        high = int64(vmath.max(float64(high), float64(workspace.material.meta[i * 5])));
        i = i + 1;
    }
    i = 0;
    while i < workspace.graph.state[0] {
        high = int64(vmath.max(float64(high), float64(workspace.graph.meta[i * 8 + 6])));
        i = i + 1;
    }
    if high >= 1000000 {
        throw "Stable parameter ID capacity exhausted";
    }
    return high + 1;
}

# Begin an exposure draft for a selected supported reflected uniform.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     row (Int64): Zero-based displayed/schema/diagnostic row index.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
#
# Errors:
#     Compile the current source before exposing uniforms.
#     Unknown reflected uniform.
#     Reflected uniform is unsupported.
fn ss_workspace_expose_begin(workspace :: SSWorkspace, row :: Int64) -> Bool {
    try {
        if workspace.panel.state[2] != 0
            || ss_document_compile_dirty(workspace.document)
            || workspace.state[9] != 0 {
            throw "Compile the current source before exposing uniforms";
        }
        if !ss_workspace_commit_field(workspace) {
            return false;
        }
        if row < 0 || row >= workspace.panel.state[0] {
            throw "Unknown reflected uniform";
        }
        if workspace.panel.reflection[row * 4 + 3] != 0 {
            throw "Reflected uniform is unsupported";
        }
        ss_schema_begin(
            workspace.panel,
            workspace.document,
            ss_workspace_new_parameter_id(workspace),
            ss_panel_name(workspace.panel, row),
            workspace.panel.reflection[row * 4],
            workspace.panel.reflection[row * 4 + 1],
            workspace.panel.reflection[row * 4 + 2],
            false
        );
        workspace.panel.state[12] = 2;
        workspace.panel.state[11] = 0;
        workspace.state[55] = 1;
        workspace.fragment.state[4] = 0;
        workspace.vertex.state[4] = 0;
        return true;
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Begin editing an existing exposed parameter's retained schema metadata.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     id (Int64): Stable identifier or generation-checked scene handle, according to the containing
#         API.
#
# Returns:
#     false when workspace.panel.state[2] != 0; false when !ss_workspace_commit_field(workspace).
#     Otherwise returns the completed action/predicate result.
#
# Errors:
#     Unknown schema parameter.
#     Uniform block / negative location cannot be exposed.
fn ss_workspace_schema_begin(workspace :: SSWorkspace, id :: Int64) -> Bool {
    try {
        if workspace.panel.state[2] != 0 {
            return false;
        }
        if !ss_workspace_commit_field(workspace) {
            return false;
        }
        p :: SSParameters := workspace.document.parameters;
        slot :: Int64 := ss_parameter_slot(p, id);
        if slot < 0 {
            throw "Unknown schema parameter";
        }
        gl :: Int64 := p.meta[slot * 8 + 1];
        count :: Int64 := p.meta[slot * 8 + 2];
        location :: Int64 := p.meta[slot * 8 + 7];
        name :: String := se_text_read(p.names[slot]);
        workspace.panel.draft[2] = gl;
        workspace.panel.draft[3] = count;
        workspace.panel.draft[9] = location;
        workspace.panel.draft[10] = 0;
        @bounded<1048576> region shader_ui_schema_reflection {
            if ss_program(workspace.document).valid() {
                rows :: Array := ss_program(workspace.document).uniforms();
                i :: Int64 := 0;
                while i < rows.size() {
                    row :: Array := rows[i];
                    if ss_uniform_base(row[0]) == name {
                        workspace.panel.draft[2] = int64(row[1]);
                        workspace.panel.draft[3] = int64(row[2]);
                        workspace.panel.draft[9] = int64(row[3]);
                        workspace.panel.draft[10] = 1;
                    }
                    i = i + 1;
                }
            }
        };
        gl = workspace.panel.draft[2];
        count = workspace.panel.draft[3];
        location = workspace.panel.draft[9];
        # Optimized-out resource metadata is still editable; no upload happens here.
        if location < 0 {
            if workspace.panel.draft[10] == 1 {
                throw "Uniform block / negative location cannot be exposed";
            }
            location = 0;
        }
        ss_schema_begin(workspace.panel, workspace.document, id, name, gl, count, location, true);
        workspace.panel.state[12] = 2;
        workspace.panel.state[11] = 0;
        workspace.state[55] = 1;
        return true;
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Validate and commit the complete schema draft, refreshing controls and reflection status as needed.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     false when !ss_workspace_commit_field(workspace). Otherwise returns the completed
#     action/predicate result.
fn ss_workspace_schema_apply(app :: vraylib.App, workspace :: SSWorkspace) -> Bool {
    if !ss_workspace_commit_field(workspace) {
        return false;
    }
    try {
        ss_schema_commit(workspace.panel, workspace.document, workspace.material);
        ss_value_history_clear(workspace.panel);
        workspace.panel.state[12] = 0;
        workspace.state[55] = 1;
        return ss_workspace_compile(app, workspace);
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Open a retained Save As path draft while preserving current editable identity.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     false when workspace.panel.draft[0] != 0 || workspace.panel.state[2] != 0; false when
#     !ss_workspace_commit_field(workspace). Otherwise returns the completed action/predicate result.
fn ss_workspace_save_as_begin(workspace :: SSWorkspace) -> Bool {
    if workspace.panel.draft[0] != 0 || workspace.panel.state[2] != 0 {
        return false;
    }
    if !ss_workspace_commit_field(workspace) {
        return false;
    }
    workspace.panel.state[2] = 1;
    workspace.state[55] = 1;
    se_edit_begin(workspace.parameter_editor, 900, -1, se_text_read(workspace.document.path));
    workspace.fragment.state[4] = 0;
    workspace.vertex.state[4] = 0;
    return true;
}

# Resolve and validate the proposed mutable shader destination path.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_workspace_save_as_path(workspace :: SSWorkspace) -> String {
    return se_text_read(workspace.parameter_editor.edit_buffer);
}

# Queue a hosted Save As request or perform the standalone checked save.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     overwrite (Bool): Whether the user has confirmed replacing an existing Save As destination.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
#
# Errors:
#     Invalid Save As path.
#     Save As layout write failed; source identity retained.
#     Save As source write failed.
fn ss_workspace_save_as_request(workspace :: SSWorkspace, path :: String, overwrite :: Bool) -> Bool {
    try {
        if path.size() == 0 || path.size() > 2048 || !ss_code_string_valid(path) {
            throw "Invalid Save As path";
        }
        se_text_write(workspace.parameter_editor.edit_buffer, path);
        workspace.state[55] = 1;
        if !overwrite && path != se_text_read(workspace.document.path) && vfs.exists(path) {
            workspace.panel.state[2] = 2;
            return false;
        }
        if workspace.state[42] != 0 {
            workspace.state[47] = 1;
            workspace.panel.state[3] = int64(overwrite);
            workspace.panel.state[2] = 3;
            workspace.parameter_editor.state[1] = -1;
            return true;
        }
        if !ss_workspace_save_layout(workspace) {
            throw "Save As layout write failed; source identity retained";
        }
        @bounded<67108864> region shader_ui_save_as {
            candidate :: SSDocument := ss_document();
            se_text_copy(candidate.source, workspace.document.source);
            se_text_copy(candidate.vertex, workspace.document.vertex);
            ss_parameters_copy(candidate.parameters, workspace.document.parameters);
            se_text_copy(candidate.fragment_path, workspace.document.fragment_path);
            se_text_copy(candidate.vertex_path, workspace.document.vertex_path);
            # Moving a resource must not silently change its relative include roots.
            if candidate.fragment_path[0] == 0 {
                se_text_copy(candidate.fragment_path, workspace.document.path);
            }
            if candidate.vertex_path[0] == 0 && candidate.vertex[0] > 0 {
                se_text_copy(candidate.vertex_path, workspace.document.path);
            }
            if !ss_document_save(candidate, path) {
                throw "Save As source write failed";
            }
            se_text_copy(workspace.document.path, candidate.path);
            se_text_copy(workspace.document.fragment_path, candidate.fragment_path);
            se_text_copy(workspace.document.vertex_path, candidate.vertex_path);
            workspace.document.state[7] = candidate.state[7];
            workspace.document.state[1] = workspace.document.state[0];
            workspace.document.state[6] = 0;
        };
        se_text_copy(workspace.saved_source, workspace.document.source);
        se_text_copy(workspace.saved_vertex, workspace.document.vertex);
        workspace.panel.state[2] = 0;
        workspace.parameter_editor.state[1] = -1;
        workspace.state[33] = 0;
        return true;
    } catch (error) {
        workspace.panel.state[2] = 1;
        workspace.state[33] = 1;
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Host acknowledges only after its source + material checkpoint transaction.

# Finish Save As UI state only after the host reports its source/material transaction result.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     success (Bool): Whether the host completed the pending transactional action.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_save_as_complete(workspace :: SSWorkspace, success :: Bool) -> Bool {
    workspace.state[47] = 0;
    workspace.state[55] = 1;
    if success {
        workspace.panel.state[2] = 0;
        workspace.parameter_editor.state[1] = -1;
        return true;
    }
    path :: String := ss_workspace_save_as_path(workspace);
    workspace.panel.state[2] = 1;
    se_edit_begin(workspace.parameter_editor, 900, -1, path);
    return false;
}

# Request checked reload of saved source, prompting when local edits or pending entry require a
# decision.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     discard (Bool): Whether explicit reload may discard local draft edits.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_reload(workspace :: SSWorkspace, discard :: Bool) -> Bool {
    if !discard
        && (ss_document_dirty(workspace.document) || workspace.parameter_editor.state[1] >= 0 || workspace.panel.draft[0] != 0) {
        workspace.document.state[6] = 1;
        workspace.panel.state[2] = 4;
        workspace.state[55] = 1;
        return false;
    }
    if workspace.state[42] != 0 {
        workspace.state[53] = 1;
        workspace.panel.state[3] = int64(discard);
        workspace.panel.state[2] = 5;
        workspace.state[55] = 1;
        return true;
    }
    try {
        @bounded<67108864> region shader_ui_reload {
            candidate :: SSDocument := ss_document();
            ss_document_load(candidate, se_text_read(workspace.document.path));
            material :: MaterialInstance := ss_material_like(workspace.material);
            ss_material_migrate(material, candidate);
            se_text_copy(workspace.document.source, candidate.source);
            se_text_copy(workspace.document.vertex, candidate.vertex);
            se_text_copy(workspace.document.fragment_path, candidate.fragment_path);
            se_text_copy(workspace.document.vertex_path, candidate.vertex_path);
            ss_parameters_copy(workspace.document.parameters, candidate.parameters);
            ss_material_copy(workspace.material, material);
            ss_document_source_change(workspace.document);
            workspace.document.state[8] = workspace.document.state[8] + 1;
            workspace.document.state[1] = workspace.document.state[0];
            workspace.document.state[7] = candidate.state[7];
            workspace.document.state[6] = 0;
        };
        se_text_copy(workspace.saved_source, workspace.document.source);
        se_text_copy(workspace.saved_vertex, workspace.document.vertex);
        ss_workspace_sync_source(workspace);
        ss_code_reset_history(workspace.fragment);
        ss_code_reset_history(workspace.vertex);
        ss_value_history_clear(workspace.panel);
        workspace.panel.draft[0] = 0;
        workspace.parameter_editor.state[1] = -1;
        workspace.panel.state[2] = 0;
        workspace.state[55] = 1;
        ss_workspace_compile(vraylib.App(workspace.state[0]), workspace);
        return true;
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
}

# Finish reload UI/editor synchronization according to host success without discarding failed pending
# state.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     success (Bool): Whether the host completed the pending transactional action.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_reload_complete(workspace :: SSWorkspace, success :: Bool) -> Bool {
    workspace.state[53] = 0;
    workspace.state[55] = 1;
    if !success {
        workspace.panel.state[2] = 4;
        return false;
    }
    workspace.panel.state[2] = 0;
    workspace.panel.draft[0] = 0;
    workspace.parameter_editor.state[1] = -1;
    ss_value_history_clear(workspace.panel);
    ss_code_reset_history(workspace.fragment);
    ss_code_reset_history(workspace.vertex);
    return true;
}

# Draw cached include diagnostic context with file identity and line navigation.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     path (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#     line (Int64): Source line index for navigation, or serialized record text for parsing.
#     column (Int64): Source column used for caret/diagnostic navigation.
#
# Returns:
#     The Boolean result of success.
#
# Errors:
#     Include viewer limit: 64 KiB.
#     Include viewer requires UTF-8 text up to 64 KiB.
fn ss_workspace_include_view(
    workspace :: SSWorkspace,
    path :: String,
    line :: Int64,
    column :: Int64
) -> Bool {
    se_text_write(workspace.diagnostic_open_path, path);
    workspace.state[48] = line;
    workspace.state[49] = column;
    workspace.panel.state[4] = 1;
    workspace.panel.state[5] = 0;
    workspace.panel.state[7] = workspace.panel.state[7] + 1;
    workspace.layout.state[3] = 4;
    workspace.layout.state[6] = 1;
    success :: Bool := false;
    @bounded<16777216> region shader_ui_include_excerpt {
        try {
            if vraylib.get_file_length(path) > 65536 {
                throw "Include viewer limit: 64 KiB";
            }
            text :: String := vfs.read(path);
            if text.size() > 65536 || !ss_code_string_valid(text) {
                throw "Include viewer requires UTF-8 text up to 64 KiB";
            }
            total :: Int64 := 1;
            scan :: Int64 := 0;
            while scan < text.size() {
                if text.byte_at(scan) == 10 {
                    total = total + 1;
                }
                scan = scan + 1;
            }
            workspace.panel.state[23] = total;
            first :: Int64 := int64(vmath.clamp(float64(line - 3), 1.0, vmath.max(1.0, float64(total - 15))));
            workspace.panel.state[6] = first;
            last :: Int64 := first + 15;
            number :: Int64 := 1;
            start :: Int64 := 0;
            i :: Int64 := 0;
            while i <= text.size() && number <= last {
                newline :: Bool := i == text.size();
                if i < text.size() {
                    if text.byte_at(i) == 10 {
                        newline = true;
                    }
                }
                if newline {
                    if number >= first {
                        row :: Int64 := workspace.panel.state[5];
                        builder :: Int64 := vcore.sb_create();
                        prefix :: String := "  ";
                        if number == line {
                            prefix = "> ";
                        }
                        vcore.sb_append(builder, prefix + string(number) + "  ");
                        j :: Int64 := start;
                        count :: Int64 := 0;
                        while j < i && count < 480 {
                            byte :: Int64 := text.byte_at(j);
                            if byte == 13 {
                                j = j + 1;
                            } else {
                                width :: Int64 := 1;
                                if byte >= 194 {
                                    width = 2;
                                }
                                if byte >= 224 {
                                    width = 3;
                                }
                                if byte >= 240 {
                                    width = 4;
                                }
                                if count + width > 480 {
                                    j = i;
                                } else {
                                    k :: Int64 := 0;
                                    while k < width {
                                        vcore.sb_append_char(builder, text.byte_at(j + k));
                                        k = k + 1;
                                    }
                                    j = j + width;
                                    count = count + width;
                                }
                            }
                        }
                        rendered :: String := vcore.sb_build(builder);
                        length :: Int64 := rendered.size();
                        workspace.messages[row * 513] = length;
                        j = 0;
                        while j < length {
                            workspace.messages[row * 513 + j + 1] = rendered.byte_at(j);
                            j = j + 1;
                        }
                        workspace.panel.state[5] = row + 1;
                    }
                    number = number + 1;
                    start = i + 1;
                }
                i = i + 1;
            }
            notice :: String := "Read-only: " + path + ":" + string(line) + ":" + string(column);
            if line > total {
                notice = notice + " / line is outside this file; showing final context";
            }
            se_text_write(workspace.diagnostic_notice, notice);
            success = true;
        } catch (error) {
            se_text_write(workspace.diagnostic_notice, "Include view: " + string(error));
        }
    };
    return success;
}

# Draw reflected uniform rows with support status and Expose controls.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_uniform_list(workspace :: SSWorkspace, box :: vraylib.Rectangle) -> Bool {
    if ss_theme_input[0] != 0 && se_hover(box) {
        workspace.panel.state[13] = int64(vmath.max(
            0.0,
            float64(workspace.panel.state[13]) - vraylib.get_mouse_wheel_move() * 2.0
        ));
    }
    y :: Float64 := box.y + ss_scale(74.0);
    i :: Int64 := workspace.panel.state[13];
    if workspace.panel.state[0] == 0 {
        se_draw_text(
            "Compile new uniforms to expose them here.",
            int64(box.x + ss_scale(8.0)),
            int64(y),
            ss_font_size(),
            ss_color(5)
        );
    }
    while i < workspace.panel.state[0] && y < box.y + box.height {
        name :: String := ss_panel_name(workspace.panel, i);
        gl :: Int64 := workspace.panel.reflection[i * 4];
        count :: Int64 := workspace.panel.reflection[i * 4 + 1];
        code :: Int64 := workspace.panel.reflection[i * 4 + 3];
        se_draw_heading(
            se_fit(name, box.width - ss_scale(16.0), ss_font_size()),
            int64(box.x + ss_scale(8.0)),
            int64(y),
            ss_font_size(),
            ss_color(4)
        );
        y = y + ss_scale(23.0);
        ss_mono(
            "GL "
                +string(gl)
                +" x"
                +string(count)
                +" / loc "
                +string(workspace.panel.reflection[i * 4 + 2]),
            box.x + ss_scale(8.0),
            y,
            ss_color(5)
        );
        y = y + ss_scale(23.0);
        se_draw_text(
            se_fit(ss_uniform_support_text(code), box.width - ss_scale(16.0), int64(ss_scale(12.0))),
            int64(box.x + ss_scale(8.0)),
            int64(y),
            int64(ss_scale(12.0)),
            ss_color(5)
        );
        y = y + ss_scale(23.0);
        if ss_workspace_action(
            workspace,
            "Expose uniform",
            se_rect(box.x + ss_scale(8.0), y, box.width - ss_scale(16.0), ss_scale(28.0)),
            code == 0 && !ss_document_compile_dirty(workspace.document) && workspace.state[9] == 0
        ) {
            ss_workspace_expose_begin(workspace, i);
        }
        y = y + ss_scale(44.0);
        i = i + 1;
    }
    if workspace.panel.state[1] != 0 {
        se_draw_text(
            "Reflection cache full: first 64 unexposed rows.",
            int64(box.x + ss_scale(8.0)),
            int64(box.y + box.height - ss_scale(22.0)),
            int64(ss_scale(12.0)),
            ss_color(7)
        );
    }
    return true;
}

# Draw and dispatch one labeled schema metadata field using precise-entry focus.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     label (String): Text displayed by the control.
#     value (String): Candidate property/parameter value or display text, as typed below.
#     field (Int64): Zero-based numeric property/component or UI field identifier.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_schema_field(
    workspace :: SSWorkspace,
    label :: String,
    value :: String,
    field :: Int64,
    box :: vraylib.Rectangle
) -> Bool {
    focused :: Bool := workspace.parameter_editor.state[1] == field;
    shown :: String := value;
    if focused {
        shown = se_text_read(workspace.parameter_editor.text);
    }
    if ss_theme_input[0] != 0 && se_hover(box) {
        workspace.panel.state[15] = 10;
        workspace.values[24] = box.x;
        workspace.values[25] = box.y + box.height;
    }
    if se_field(label, shown, box, focused, true) && ss_theme_input[0] != 0 {
        if !focused {
            if !ss_workspace_commit_field(workspace) {
                return false;
            }
            se_edit_begin(workspace.parameter_editor, field, workspace.panel.draft[1], value);
            if field == 100 || field == 101 {
                ss_workspace_number_baseline(
                    workspace,
                    field,
                    workspace.panel.draft[1],
                    field - 100,
                    workspace.panel.numbers[field - 100]
                );
            } else {
                if field >= 200 {
                    ss_workspace_number_baseline(
                        workspace,
                        field,
                        workspace.panel.draft[1],
                        field - 200,
                        workspace.panel.numbers[field - 198]
                    );
                } else {
                    workspace.panel.state[18] = -1;
                }
            }
            workspace.state[55] = 1;
        }
    }
    return true;
}

# Draw the typed schema/default/range/group/units exposure/edit form and its Apply/Cancel actions.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_schema_form(workspace :: SSWorkspace, box :: vraylib.Rectangle) -> Bool {
    x :: Float64 := box.x + ss_scale(8.0);
    width :: Float64 := vmath.max(0.0, box.width - ss_scale(16.0));
    y :: Float64 := box.y + ss_scale(74.0);
    se_draw_heading(
        se_fit(
            se_text_read(workspace.panel.name) + " #" + string(workspace.panel.draft[1]),
            width,
            ss_font_size()
        ),
        int64(x),
        int64(y),
        ss_font_size(),
        ss_color(4)
    );
    y = y + ss_scale(24.0);
    ss_mono(
        "GL "
            +string(workspace.panel.draft[2])
            +" x"
            +string(workspace.panel.draft[3])
            +" / "
            +string(workspace.panel.draft[4])
            +" components",
        x,
        y,
        ss_color(5)
    );
    y = y + ss_scale(26.0);
    if ss_theme_input[0] != 0 && se_hover(box) {
        workspace.panel.state[11] = int64(vmath.max(
            0.0,
            float64(workspace.panel.state[11]) - vraylib.get_mouse_wheel_move() * 2.0
        ));
    }
    top :: Float64 := y;
    y = y - float64(workspace.panel.state[11]) * ss_scale(30.0);
    bottom :: Float64 := box.y + box.height - ss_scale(42.0);
    if y >= top && y < bottom {
        ss_workspace_schema_field(
            workspace,
            "Minimum",
            string(workspace.panel.numbers[0]),
            100,
            se_rect(x, y, width, ss_scale(26.0))
        );
    }
    y = y + ss_scale(30.0);
    if y >= top && y < bottom {
        ss_workspace_schema_field(
            workspace,
            "Maximum",
            string(workspace.panel.numbers[1]),
            101,
            se_rect(x, y, width, ss_scale(26.0))
        );
    }
    y = y + ss_scale(30.0);
    if y >= top && y < bottom {
        ss_workspace_schema_field(
            workspace,
            "Group",
            se_text_read(workspace.panel.group_name),
            102,
            se_rect(x, y, width, ss_scale(26.0))
        );
    }
    y = y + ss_scale(30.0);
    if y >= top && y < bottom {
        ss_workspace_schema_field(
            workspace,
            "Units",
            se_text_read(workspace.panel.units),
            103,
            se_rect(x, y, width, ss_scale(26.0))
        );
    }
    y = y + ss_scale(30.0);
    if y >= top && y < bottom {
        if ss_tab(
            "Continuous",
            se_rect(x, y, width * 0.6, ss_scale(28.0)),
            workspace.panel.draft[5] != 0
        ) && ss_continuous(workspace.panel.draft[2]) {
            workspace.panel.draft[5] = 1 - workspace.panel.draft[5];
            if workspace.panel.draft[5] == 0 {
                workspace.panel.draft[6] = 0;
            }
        }
        if ss_tab(
            "Fit",
            se_rect(x + width * 0.6, y, width * 0.4, ss_scale(28.0)),
            workspace.panel.draft[6] != 0
        ) && workspace.panel.draft[5] != 0 {
            workspace.panel.draft[6] = 1 - workspace.panel.draft[6];
        }
    }
    y = y + ss_scale(34.0);
    i :: Int64 := 0;
    while i < workspace.panel.draft[4] {
        if y >= top && y < bottom {
            ss_workspace_schema_field(
                workspace,
                "Default [" + string(i) + "]",
                string(workspace.panel.numbers[i + 2]),
                200 + i,
                se_rect(x, y, width, ss_scale(26.0))
            );
        }
        y = y + ss_scale(30.0);
        i = i + 1;
    }
    if ss_workspace_action(
        workspace,
        "Apply schema",
        se_rect(x, box.y + box.height - ss_scale(34.0), width * 0.65, ss_scale(28.0)),
        true
    ) {
        ss_workspace_schema_apply(vraylib.App(workspace.state[0]), workspace);
    }
    if ss_workspace_action(
        workspace,
        "Cancel",
        se_rect(x + width * 0.67, box.y + box.height - ss_scale(34.0), width * 0.33, ss_scale(28.0)),
        true
    ) {
        workspace.panel.draft[0] = 0;
        workspace.parameter_editor.state[1] = -1;
        workspace.panel.state[12] = 1;
        workspace.state[55] = 1;
    }
    return true;
}

# Draw workspace precise-entry, exposure, Save As, Reload, and related modal dialogs.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     bounds (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     false when mode == 0. Otherwise returns the completed action/predicate result.
fn ss_workspace_dialog_draw(workspace :: SSWorkspace, bounds :: vraylib.Rectangle) -> Bool {
    mode :: Int64 := workspace.panel.state[2];
    if mode == 0 {
        return false;
    }
    vraylib.draw_rectangle_rec(bounds, vraylib.rgba(8, 11, 16, 190));
    width :: Float64 := vmath.min(bounds.width - ss_scale(24.0), ss_scale(640.0));
    height :: Float64 := ss_scale(176.0);
    box :: vraylib.Rectangle := se_rect(
        bounds.x + (bounds.width - width) * 0.5,
        bounds.y + (bounds.height - height) * 0.5,
        width,
        height
    );
    vraylib.draw_rectangle_rec(box, ss_color(1));
    vraylib.draw_rectangle_lines_ex(box, ss_scale(1.0), ss_color(6));
    x :: Float64 := box.x + ss_scale(14.0);
    y :: Float64 := box.y + ss_scale(12.0);
    title :: String := "Save shader resource as";
    if mode >= 4 {
        title = "Reload shader resource";
    }
    se_draw_heading(title, int64(x), int64(y), ss_font_size(), ss_color(4));
    y = y + ss_scale(32.0);
    if mode == 1 {
        if workspace.parameter_editor.state[1] != 900 {
            se_edit_begin(workspace.parameter_editor, 900, -1, ss_workspace_save_as_path(workspace));
        }
        se_field(
            "Path",
            se_text_read(workspace.parameter_editor.text),
            se_rect(x, y, width - ss_scale(28.0), ss_scale(32.0)),
            true,
            true
        );
    } else {
        text :: String := ss_workspace_save_as_path(workspace);
        if mode == 2 {
            text = "Replace existing: " + text;
        }
        if mode == 3 {
            text = "Waiting for host checkpoint: " + text;
        }
        if mode == 4 {
            text = "Discard local source, values and metadata; reload disk?";
        }
        if mode == 5 {
            text = "Waiting for host source/material reload";
        }
        se_draw_text(
            se_fit(text, width - ss_scale(28.0), ss_font_size()),
            int64(x),
            int64(y),
            ss_font_size(),
            ss_color(5)
        );
    }
    y = box.y + height - ss_scale(42.0);
    if mode == 1 {
        if ss_workspace_action(workspace, "Save As", se_rect(x, y, ss_scale(100.0), ss_scale(28.0)), true) {
            ss_workspace_commit_field(workspace);
        }
    }
    if mode == 2 {
        if ss_workspace_action(workspace, "Replace", se_rect(x, y, ss_scale(100.0), ss_scale(28.0)), true) {
            ss_workspace_save_as_request(workspace, ss_workspace_save_as_path(workspace), true);
        }
    }
    if mode == 4 {
        if ss_workspace_action(
            workspace,
            "Discard / Reload",
            se_rect(x, y, ss_scale(154.0), ss_scale(28.0)),
            true
        ) {
            ss_workspace_reload(workspace, true);
        }
    }
    if mode != 3 && mode != 5 {
        if ss_workspace_action(
            workspace,
            "Cancel",
            se_rect(box.x + width - ss_scale(100.0), y, ss_scale(86.0), ss_scale(28.0)),
            true
        ) {
            workspace.panel.state[2] = 0;
            workspace.parameter_editor.state[1] = -1;
            workspace.document.state[6] = 0;
            workspace.state[55] = 1;
        }
    }
    return true;
}

# Draw a bounded contextual tooltip for the hovered Shader Studio control.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     bounds (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_tooltip(workspace :: SSWorkspace, bounds :: vraylib.Rectangle) -> Bool {
    tip :: Int64 := workspace.panel.state[15];
    if tip != workspace.panel.state[16] {
        workspace.panel.state[16] = tip;
        workspace.values[26] = vraylib.get_time();
    }
    if tip == 0 || vraylib.get_time() - workspace.values[26] < 0.35 {
        return false;
    }
    text :: String := "Click to edit; Enter stages the field. Apply schema validates all components.";
    if tip == 1 {
        text = "Compile current source / Ctrl+Enter";
    }
    if tip == 2 {
        text = "Save source and the hosted material checkpoint / Ctrl+S";
    }
    if tip == 3 {
        text = "Save under a new resource identity / Ctrl+Shift+S";
    }
    if tip == 4 {
        text = "Reload disk; local source, values and drafts require explicit discard";
    }
    if tip == 5 {
        text = "Define stable typed defaults, ranges, group, units and fitting flags";
    }
    if tip == 6 {
        text = "Validate the entire draft, migrate compatible values, then compile bindings";
    }
    if tip == 7 {
        text = "Undo/redo one parameter stroke or reset; source has its own history";
    }
    if tip == 8 {
        text = "Optional host external-editor event; the read-only excerpt already works here";
    }
    if tip == 9 {
        text = "Exact linear RGBA reference copy; freeze time before comparing";
    }
    size :: Int64 := int64(ss_scale(12.0));
    width :: Float64 := vmath.min(bounds.width - ss_scale(16.0), se_measure(text, size) + ss_scale(16.0));
    x :: Float64 := vmath.clamp(
        workspace.values[24],
        bounds.x + ss_scale(8.0),
        vmath.max(bounds.x + ss_scale(8.0), bounds.x + bounds.width - width - ss_scale(8.0))
    );
    y :: Float64 := vmath.min(
        workspace.values[25] + ss_scale(6.0),
        bounds.y + bounds.height - ss_scale(32.0)
    );
    box :: vraylib.Rectangle := se_rect(x, y, width, ss_scale(28.0));
    vraylib.draw_rectangle_rec(box, ss_color(2));
    vraylib.draw_rectangle_lines_ex(box, ss_scale(1.0), ss_color(3));
    se_draw_text(
        se_fit(text, width - ss_scale(16.0), size),
        int64(x + ss_scale(8.0)),
        int64(y + ss_scale(7.0)),
        size,
        ss_color(4)
    );
    return true;
}

# Draw geometry, time freeze/step, resolution, reference capture, and pixel-inspection controls.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_preview_controls(workspace :: SSWorkspace, box :: vraylib.Rectangle) -> Bool {
    y :: Float64 := box.y + ss_scale(42.0);
    x :: Float64 := box.x + ss_scale(8.0);
    width :: Float64 := vmath.max(0.0, box.width - ss_scale(16.0));
    shapes := ["Plane", "Cube", "Sphere", "Selected model"];
    i :: Int64 := 0;
    while i < 4 {
        if ss_tab(
            shapes[i],
            se_rect(x, y, width, ss_scale(30.0)),
            workspace.state[35] == i
        ) && (i < 3 || workspace.state[8] != 0) {
            workspace.state[35] = i;
        }
        y = y + ss_scale(32.0);
        i = i + 1;
    }
    label :: String := "Freeze time";
    if workspace.state[14] != 0 {
        label = "Resume time";
    }
    if ss_workspace_action(workspace, label, se_rect(x, y, width, ss_scale(28.0)), true) {
        workspace.state[14] = 1 - workspace.state[14];
    }
    y = y + ss_scale(32.0);
    if ss_workspace_action(
        workspace,
        "Step +1/60 s",
        se_rect(x, y, width, ss_scale(28.0)),
        workspace.state[14] != 0
    ) {
        workspace.values[0] = workspace.values[0] + 1.0 / 60.0;
    }
    y = y + ss_scale(36.0);
    sizes :: Array<Int64> := [128, 256, 512];
    i = 0;
    while i < 3 {
        if ss_tab(
            string(sizes[i]),
            se_rect(x + float64(i) * width / 3.0, y, width / 3.0, ss_scale(30.0)),
            workspace.state[15] == sizes[i]
        ) {
            workspace.state[15] = sizes[i];
        }
        i = i + 1;
    }
    y = y + ss_scale(38.0);
    if ss_workspace_action(workspace, "Capture reference", se_rect(x, y, width, ss_scale(28.0)), true) {
        workspace.state[12] = 1;
    }
    y = y + ss_scale(34.0);
    modes := ["Live", "Reference", "Difference x4"];
    i = 0;
    while i < 3 {
        if ss_tab(
            modes[i],
            se_rect(x, y, width, ss_scale(30.0)),
            workspace.state[13] == i
        ) && (i == 0 || workspace.state[26] != 0) {
            workspace.state[13] = i;
        }
        y = y + ss_scale(32.0);
        i = i + 1;
    }
    if ss_workspace_action(
        workspace,
        "Measure difference",
        se_rect(x, y, width, ss_scale(28.0)),
        workspace.state[26] != 0
    ) {
        workspace.state[27] = 1;
    }
    y = y + ss_scale(34.0);
    ss_mono(
        "peak " + string(workspace.values[3]) + " / rms " + string(workspace.values[4]),
        x,
        y,
        ss_color(5)
    );
    return true;
}

# Draw the active Shader/Rendering workspace panels, source/graph content, parameters, diagnostics,
# and preview.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     bounds (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_draw(app :: vraylib.App, workspace :: SSWorkspace, bounds :: vraylib.Rectangle) -> Bool {
    ss_theme_input[0] = int64(workspace.panel.state[2] == 0);
    workspace.panel.state[15] = 0;
    ss_theme_dpi(workspace.layout.values[3]);
    navigation :: vraylib.Rectangle := ss_layout_region(workspace.layout, bounds, 0);
    vraylib.draw_rectangle_rec(
        se_rect(
            bounds.x,
            bounds.y + navigation.height,
            bounds.width,
            vmath.max(0.0, bounds.height - navigation.height)
        ),
        ss_color(3)
    );
    ss_workspace_toolbar(app, workspace, bounds);
    source :: vraylib.Rectangle := ss_layout_region(workspace.layout, bounds, 2);
    body :: vraylib.Rectangle := ss_workspace_source_box(workspace, bounds);
    if source.width > 0.0 {
        labels := ["Fragment", "Vertex", "Graph", "Pipeline"];
        i :: Int64 := 0;
        while i < 4 {
            if ss_tab(
                labels[i],
                se_rect(
                    source.x + float64(i) * source.width / 4.0,
                    source.y,
                    source.width / 4.0,
                    ss_scale(32.0)
                ),
                workspace.layout.state[1] == i
            ) {
                workspace.layout.state[1] = i;
                workspace.layout.state[6] = 1;
                workspace.fragment.state[4] = 0;
                workspace.vertex.state[4] = 0;
            }
            i = i + 1;
        }
        if workspace.layout.state[1] < 2 {
            count :: Int64 := ss_workspace_stage_diagnostics(workspace, workspace.layout.state[1]);
            ss_code_draw(app, ss_workspace_editor(workspace), body, workspace.visible_diagnostics, count);
        }
        if workspace.layout.state[1] == 2 {
            ss_workspace_graph_panel(app, workspace, body);
        }
        if workspace.layout.state[1] == 3 {
            ss_workspace_pipeline_panel(app, workspace, body);
        }
    }
    preview :: vraylib.Rectangle := ss_layout_region(workspace.layout, bounds, 3);
    if preview.width > 0.0 && preview.height > 0.0 {
        vraylib.draw_rectangle_rec(preview, ss_color(0));
        texture_box :: vraylib.Rectangle := ss_workspace_preview_box(preview);
        token :: Int64 := workspace.state[25];
        if workspace.state[13] == 1 && workspace.state[26] != 0 {
            token = workspace.state[2];
        }
        if workspace.state[13] == 2 && workspace.state[26] != 0 {
            token = workspace.state[3];
        }
        if token != 0 {
            target :: vraylib.RenderTarget := vraylib.RenderTarget(token, app.token);
            target.texture().draw_region(
                se_rect(0.0, 0.0, float64(target.width()), float64(target.height())),
                texture_box,
                vraylib.vec2(0.0, 0.0),
                0.0,
                vraylib.WHITE
            );
        }
        se_draw_heading(
            "PREVIEW / " + ss_workspace_status(workspace),
            int64(preview.x + ss_scale(10.0)),
            int64(preview.y + ss_scale(10.0)),
            ss_font_size(),
            ss_color(5)
        );
        if ss_theme_input[0] != 0
            && se_hover(texture_box)
            && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT)
            && workspace.state[25] != 0 {
            mouse :: vraylib.Vector2 := vraylib.get_mouse_position();
            output :: vraylib.RenderTarget := vraylib.RenderTarget(workspace.state[25], app.token);
            workspace.values[5] = vmath.floor((mouse.x - texture_box.x) / texture_box.width * float64(output.width()));
            workspace.values[6] = vmath.floor((mouse.y - texture_box.y) / texture_box.height * float64(output.height()));
            workspace.state[27] = 1;
        }
        ss_mono(
            string(workspace.state[15])
                +" x "
                +string(workspace.state[15])
                +"   t "
                +string(workspace.values[0]),
            preview.x + ss_scale(10.0),
            preview.y + preview.height - ss_scale(25.0),
            ss_color(5)
        );
    }
    inspector :: vraylib.Rectangle := ss_layout_region(workspace.layout, bounds, 4);
    if inspector.width > 0.0 && inspector.height > 0.0 {
        clip :: vraylib.Clip := app.clip(inspector);
        se_clip_start(clip, inspector);
        try {
            vraylib.draw_rectangle_rec(inspector, ss_color(1));
            if ss_tab(
                "Parameters",
                se_rect(inspector.x, inspector.y, inspector.width * 0.5, ss_scale(32.0)),
                workspace.layout.state[2] == 0
            ) {
                workspace.layout.state[2] = 0;
                workspace.layout.state[6] = 1;
            }
            if ss_tab(
                "Preview",
                se_rect(
                    inspector.x + inspector.width * 0.5,
                    inspector.y,
                    inspector.width * 0.5,
                    ss_scale(32.0)
                ),
                workspace.layout.state[2] == 1
            ) && workspace.panel.draft[0] == 0 {
                workspace.layout.state[2] = 1;
                workspace.layout.state[6] = 1;
            }
            if workspace.layout.state[2] == 0 {
                ss_workspace_parameters(workspace, inspector);
            } else {
                ss_workspace_preview_controls(workspace, inspector);
            }
        } finally {
            se_clip_finish(clip);
        }
    }
    ss_workspace_drawer(app, workspace, ss_layout_region(workspace.layout, bounds, 5));
    status :: vraylib.Rectangle := ss_layout_region(workspace.layout, bounds, 6);
    vraylib.draw_rectangle_rec(status, ss_color(1));
    editor :: SSCodeEditor := ss_workspace_editor(workspace);
    message :: String := "Ln " + string(ss_code_line(editor, editor.state[0]) + 1) + ", Col " + string(ss_code_column(
        editor,
        editor.lines[ss_code_line(editor, editor.state[0])],
        editor.state[0]
    ) + 1) + "   UTF-8 / GLSL 330   Ctrl+Enter Compile   Ctrl+S Save   F11 Preview";
    if editor.state[10] > 0 {
        message = message + "   Undo: " + string(editor.state[10]) + " oldest entries evicted";
    }
    if editor.state[9] != 0 {
        message = "Edit rejected: source capacity / invalid bytes";
    }
    if editor.state[18] != 0 {
        message = "Highlight capacity reached; remaining source uses plain text";
    }
    if workspace.state[33] != 0 {
        message = "Save failed / see Diagnostics";
    }
    se_draw_text(
        se_fit(message, status.width - ss_scale(16.0), int64(ss_scale(12.0))),
        int64(status.x + ss_scale(8.0)),
        int64(status.y + ss_scale(5.0)),
        int64(ss_scale(12.0)),
        ss_color(5)
    );
    ss_theme_input[0] = 1;
    ss_workspace_dialog_draw(workspace, bounds);
    ss_workspace_tooltip(workspace, bounds);
    return true;
}

# Compute the preview screen rectangle from workspace layout and available bounds.
#
# Parameters:
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     The computed vraylib.Rectangle value: se_rect(box.x + (box.width - size) * 0.5, box.y +
#     ss_scale(36.0) + (box.height - ss_scale(70.0) - size) * 0.5, size, size).
fn ss_workspace_preview_box(box :: vraylib.Rectangle) -> vraylib.Rectangle {
    size :: Float64 := vmath.max(0.0, vmath.min(box.width - ss_scale(20.0), box.height - ss_scale(70.0)));
    return se_rect(
        box.x + (box.width - size) * 0.5,
        box.y + ss_scale(36.0) + (box.height - ss_scale(70.0) - size) * 0.5,
        size,
        size
    );
}

# Convenience hosted API: call within app.frame before app.begin. Host draws shared
# navigation once and dispatches by its returned workspace ID.

# Advance workspace input/state and render its preview before the shared application presentation
# pass.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     bounds (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#     dt (Float64): Frame delta time in seconds.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_workspace_frame(
    app :: vraylib.App,
    workspace :: SSWorkspace,
    bounds :: vraylib.Rectangle,
    dt :: Float64
) -> Bool {
    ss_workspace_update(app, workspace, bounds, dt);
    try {
        ss_workspace_preview(app, workspace);
    } catch (error) {
        se_text_write(workspace.log, string(error));
        ss_workspace_diagnostics(workspace, workspace.log);
        return false;
    }
    return true;
}

# Return a display label for a typed graph node kind.
#
# Parameters:
#     kind (Int64): Catalog/tool/graph operation kind index used by the selected subsystem.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn ss_workspace_graph_kind(kind :: Int64) -> String {
    if kind == 0 {
        return "Constant";
    }
    if kind == 1 {
        return "Parameter";
    }
    if kind == 2 {
        return "Add";
    }
    if kind == 3 {
        return "Multiply";
    }
    if kind == 4 {
        return "Mix";
    }
    if kind == 5 {
        return "UV";
    }
    if kind == 6 {
        return "Texture sample";
    }
    if kind == 7 {
        return "Fragment output";
    }
    if kind == 8 {
        return "Subtract";
    }
    return "Divide";
}

# Draw and dispatch an individual graph node's sockets, values, position, and selection.
#
# Parameters:
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#     slot (Int64): Zero-based storage slot; it is distinct from a generation-checked scene handle.
#
# Returns:
#     The computed vraylib.Rectangle value: se_rect(box.x + ss_scale(g.values[slot * 6 + 4]) -
#     workspace.values[21], box.y + ss_scale(g.values[slot * 6 + 5]) - workspace.values[22],
#     ss_scale(174.0), ss_scale(112.0)).
fn ss_workspace_graph_node(
    workspace :: SSWorkspace,
    box :: vraylib.Rectangle,
    slot :: Int64
) -> vraylib.Rectangle {
    g :: SSGraph := workspace.graph;
    return se_rect(
        box.x + ss_scale(g.values[slot * 6 + 4]) - workspace.values[21],
        box.y + ss_scale(g.values[slot * 6 + 5]) - workspace.values[22],
        ss_scale(174.0),
        ss_scale(112.0)
    );
}

# Draw/edit the typed graph, dispatch node/connect operations, and generate GLSL through the shared
# document path.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_graph_panel(app :: vraylib.App, workspace :: SSWorkspace, box :: vraylib.Rectangle) -> Bool {
    clip :: vraylib.Clip := app.clip(box);
    se_clip_start(clip, box);
    try {
        vraylib.draw_rectangle_rec(box, ss_color(0));
        g :: SSGraph := workspace.graph;
        if ss_theme_input[0] != 0
            && se_hover(box)
            && vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_MIDDLE) {
            delta :: vraylib.Vector2 := vraylib.get_mouse_delta();
            workspace.values[21] = vmath.max(0.0, workspace.values[21] - delta.x);
            workspace.values[22] = vmath.max(0.0, workspace.values[22] - delta.y);
        }
        if ss_workspace_action(
            workspace,
            "Generate GLSL",
            se_rect(box.x + ss_scale(8.0), box.y + ss_scale(6.0), ss_scale(136.0), ss_scale(28.0)),
            true
        ) {
            if ss_graph_generate(g, workspace.document) {
                ss_code_reset_history(workspace.fragment);
                workspace.state[9] = 0;
                workspace.layout.state[1] = 0;
            } else {
                ss_workspace_diagnostics(workspace, g.log);
            }
        }
        if ss_workspace_action(
            workspace,
            "Save graph",
            se_rect(box.x + ss_scale(150.0), box.y + ss_scale(6.0), ss_scale(106.0), ss_scale(28.0)),
            true
        ) {
            try {
                ss_graph_save(
                    g,
                    vmge_option(
                        "VYNE_VMGE_SHADER_GRAPH",
                        vfs.join(se_text_read(workspace.project), "shaders/studio.vgraph")
                    )
                );
            } catch (error) {
                se_text_write(workspace.log, string(error));
                ss_workspace_diagnostics(workspace, workspace.log);
            }
        }
        kinds :: Array<Int64> := [0, 2, 3, 4, 5, 1];
        labels := ["Color", "Add", "Mul", "Mix", "UV", "Param"];
        i :: Int64 := 0;
        while i < 6 {
            if ss_workspace_action(
                workspace,
                labels[i],
                se_rect(
                    box.x + ss_scale(8.0 + float64(i) * 58.0),
                    box.y + ss_scale(40.0),
                    ss_scale(54.0),
                    ss_scale(26.0)
                ),
                g.state[0] < 64
            ) {
                id :: Int64 := workspace.state[31];
                while ss_graph_slot(g, id) >= 0 {
                    id = id + 1;
                }
                workspace.state[31] = id + 1;
                type_id :: Int64 := 4;
                if kinds[i] == 5 {
                    type_id = 2;
                }
                name :: String := labels[i];
                param :: Int64 := 0;
                if kinds[i] == 1 {
                    name = "parameter_" + string(id);
                    param = id + 100;
                }
                slot :: Int64 := ss_graph_add(g, id, kinds[i], type_id, name, param);
                ss_graph_value(g, id, 3, 1.0);
                ss_graph_value(g, id, 4, 24.0 + float64(slot % 2) * 190.0);
                ss_graph_value(g, id, 5, 90.0 + float64(slot / 2) * 130.0);
                workspace.state[21] = slot;
            }
            i = i + 1;
        }
        # Draw typed connections first; sockets remain stable IDs after dragging.
        i = 0;
        while i < g.state[0] {
            node :: vraylib.Rectangle := ss_workspace_graph_node(workspace, box, i);
            j :: Int64 := 0;
            while j < ss_graph_inputs(g.meta[i * 8 + 1]) {
                from :: Int64 := ss_graph_slot(g, g.meta[i * 8 + 3 + j]);
                if from >= 0 {
                    origin :: vraylib.Rectangle := ss_workspace_graph_node(workspace, box, from);
                    vraylib.draw_line_ex(
                        vraylib.vec2(origin.x + origin.width, origin.y + ss_scale(42.0)),
                        vraylib.vec2(node.x, node.y + ss_scale(42.0 + float64(j) * 22.0)),
                        ss_scale(2.0),
                        ss_color(6)
                    );
                }
                j = j + 1;
            }
            i = i + 1;
        }
        i = 0;
        while i < g.state[0] {
            node :: vraylib.Rectangle := ss_workspace_graph_node(workspace, box, i);
            vraylib.draw_rectangle_rec(node, ss_color(1));
            edge :: vraylib.Color := ss_color(3);
            if workspace.state[21] == i {
                edge = ss_color(6);
            }
            vraylib.draw_rectangle_lines_ex(node, ss_scale(1.0), edge);
            title :: vraylib.Rectangle := se_rect(node.x, node.y, node.width, ss_scale(28.0));
            se_draw_heading(
                ss_workspace_graph_kind(g.meta[i * 8 + 1]) + " #" + string(g.meta[i * 8]),
                int64(node.x + ss_scale(6.0)),
                int64(node.y + ss_scale(7.0)),
                int64(ss_scale(12.0)),
                ss_color(4)
            );
            if ss_theme_input[0] != 0
                && se_hover(title)
                && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT) {
                workspace.state[21] = i;
                workspace.state[22] = 1;
            }
            if ss_theme_input[0] != 0
                && workspace.state[21] == i
                && workspace.state[22] != 0
                && vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) {
                motion :: vraylib.Vector2 := vraylib.get_mouse_delta();
                ss_graph_value(
                    g,
                    g.meta[i * 8],
                    4,
                    vmath.clamp(
                        g.values[i * 6 + 4] + motion.x / workspace.layout.values[3],
                        0.0,
                        vmath.max(0.0, box.width / workspace.layout.values[3] - 174.0)
                    )
                );
                ss_graph_value(
                    g,
                    g.meta[i * 8],
                    5,
                    vmath.clamp(
                        g.values[i * 6 + 5] + motion.y / workspace.layout.values[3],
                        76.0,
                        vmath.max(76.0, box.height / workspace.layout.values[3] - 112.0)
                    )
                );
            }
            if ss_tab(
                ss_graph_type(g.meta[i * 8 + 2]) + " >",
                se_rect(
                    node.x + node.width - ss_scale(78.0),
                    node.y + ss_scale(30.0),
                    ss_scale(78.0),
                    ss_scale(24.0)
                ),
                workspace.state[23] == g.meta[i * 8]
            ) {
                workspace.state[23] = g.meta[i * 8];
            }
            j :: Int64 := 0;
            while j < ss_graph_inputs(g.meta[i * 8 + 1]) {
                label :: String := "in " + string(j) + " #" + string(g.meta[i * 8 + 3 + j]);
                if ss_tab(
                    label,
                    se_rect(
                        node.x,
                        node.y + ss_scale(32.0 + float64(j) * 22.0),
                        ss_scale(90.0),
                        ss_scale(22.0)
                    ),
                    false
                ) && workspace.state[23] >= 0 {
                    ss_graph_connect(g, g.meta[i * 8], j, workspace.state[23]);
                    workspace.state[23] = -1;
                }
                j = j + 1;
            }
            if g.meta[i * 8 + 1] == 0 || g.meta[i * 8 + 1] == 1 {
                j = 0;
                while j < g.meta[i * 8 + 2] && j < 4 {
                    component :: vraylib.Rectangle := se_rect(
                        node.x + ss_scale(6.0 + float64(j) * 40.0),
                        node.y + ss_scale(70.0),
                        ss_scale(38.0),
                        ss_scale(30.0)
                    );
                    ss_mono(string(g.values[i * 6 + j]), component.x, component.y, ss_color(5));
                    if ss_theme_input[0] != 0 && se_hover(component) {
                        wheel :: Float64 := vraylib.get_mouse_wheel_move();
                        if wheel != 0.0 {
                            ss_graph_value(g, g.meta[i * 8], j, g.values[i * 6 + j] + wheel * 0.05);
                        }
                    }
                    j = j + 1;
                }
            }
            i = i + 1;
        }
        if !vraylib.is_mouse_button_down(vraylib.MOUSE_BUTTON_LEFT) {
            workspace.state[22] = 0;
        }
        if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) {
            workspace.state[23] = -1;
        }
        ss_mono(
            "Drag titles / output then input to wire / wheel edits constants",
            box.x + ss_scale(8.0),
            box.y + box.height - ss_scale(22.0),
            ss_color(5)
        );
    } finally {
        se_clip_finish(clip);
    }
    return true;
}

# Draw/edit a standalone ordered pass list with dependency-safe shader/input controls.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_workspace_pipeline_panel(
    app :: vraylib.App,
    workspace :: SSWorkspace,
    box :: vraylib.Rectangle
) -> Bool {
    clip :: vraylib.Clip := app.clip(box);
    se_clip_start(clip, box);
    try {
        vraylib.draw_rectangle_rec(box, ss_color(0));
        p :: SSPipeline := workspace.pipeline;
        x :: Float64 := box.x + ss_scale(10.0);
        y :: Float64 := box.y + ss_scale(8.0);
        label :: String := "Enable pipeline";
        if workspace.state[24] != 0 {
            label = "Disable pipeline";
        }
        if ss_workspace_action(workspace, label, se_rect(x, y, ss_scale(154.0), ss_scale(28.0)), true) {
            workspace.state[24] = 1 - workspace.state[24];
        }
        if ss_workspace_action(
            workspace,
            "Add pass",
            se_rect(x + ss_scale(160.0), y, ss_scale(88.0), ss_scale(28.0)),
            p.state[0] < 16
        ) {
            id :: Int64 := 1;
            while ss_pipeline_slot(p, id) >= 0 {
                id = id + 1;
            }
            input :: Int64 := -1;
            if p.state[0] > 0 {
                input = p.meta[(p.state[0] - 1) * 10];
            }
            ss_pipeline_add(p, id, 2, input, -2, 0, 0, 1, 1);
        }
        if ss_workspace_action(
            workspace,
            "Save",
            se_rect(x + ss_scale(254.0), y, ss_scale(64.0), ss_scale(28.0)),
            true
        ) {
            try {
                ss_pipeline_save(
                    p,
                    vmge_option(
                        "VYNE_VMGE_SHADER_PIPELINE",
                        vfs.join(se_text_read(workspace.project), "shaders/studio.vpipeline")
                    )
                );
            } catch (error) {
                se_text_write(workspace.log, string(error));
                ss_workspace_diagnostics(workspace, workspace.log);
            }
        }
        if ss_theme_input[0] != 0 && se_hover(box) {
            workspace.state[41] = int64(vmath.clamp(
                float64(workspace.state[41]) - vraylib.get_mouse_wheel_move(),
                0.0,
                vmath.max(0.0, float64(p.state[0]) - 1.0)
            ));
        }
        y = y + ss_scale(46.0);
        i :: Int64 := workspace.state[41];
        while i < p.state[0] {
            row :: vraylib.Rectangle := se_rect(
                x,
                y,
                vmath.max(0.0, box.width - ss_scale(20.0)),
                ss_scale(106.0)
            );
            vraylib.draw_rectangle_rec(row, ss_color(1));
            se_draw_heading(
                "Pass #" + string(p.meta[i * 10]) + " / Shader #" + string(p.meta[i * 10 + 1]),
                int64(x + ss_scale(8.0)),
                int64(y + ss_scale(8.0)),
                ss_font_size(),
                ss_color(4)
            );
            ss_mono(
                "input #"
                    +string(p.meta[i * 10 + 2])
                    +" / RGBA"
                    +string(8 + 24 * p.meta[i * 10 + 8])
                    +" / divisor "
                    +string(p.meta[i * 10 + 7]),
                x + ss_scale(8.0),
                y + ss_scale(32.0),
                ss_color(5)
            );
            label = "Bypass";
            if p.meta[i * 10 + 4] == 0 {
                label = "Enable";
            }
            if ss_workspace_action(
                workspace,
                label,
                se_rect(x + ss_scale(8.0), y + ss_scale(62.0), ss_scale(70.0), ss_scale(28.0)),
                true
            ) {
                ss_pipeline_enable(p, p.meta[i * 10], p.meta[i * 10 + 4] == 0, p.meta[i * 10 + 9] != 0);
            }
            if ss_workspace_action(
                workspace,
                "Capture",
                se_rect(x + ss_scale(84.0), y + ss_scale(62.0), ss_scale(80.0), ss_scale(28.0)),
                true
            ) {
                ss_pipeline_enable(p, p.meta[i * 10], p.meta[i * 10 + 4] != 0, p.meta[i * 10 + 9] == 0);
            }
            if ss_workspace_action(
                workspace,
                "1/2 res",
                se_rect(x + ss_scale(170.0), y + ss_scale(62.0), ss_scale(84.0), ss_scale(28.0)),
                true
            ) {
                divisor :: Int64 := p.meta[i * 10 + 7] * 2;
                if divisor > 8 {
                    divisor = 1;
                }
                p.meta[i * 10 + 7] = divisor;
                p.state[1] = p.state[1] + 1;
            }
            if ss_workspace_action(
                workspace,
                "Inspect",
                se_rect(x + ss_scale(260.0), y + ss_scale(62.0), ss_scale(78.0), ss_scale(28.0)),
                p.outputs[i] != 0
            ) {
                workspace.state[30] = i;
                workspace.layout.state[3] = 3;
            }
            y = y + ss_scale(116.0);
            i = i + 1;
        }
        ss_mono(
            "Ordered passes / validated input dependencies / explicit captures",
            x,
            box.y + box.height - ss_scale(22.0),
            ss_color(5)
        );
    } finally {
        se_clip_finish(clip);
    }
    return true;
}

# Draw the lower diagnostics/reference/output drawer using retained workspace state.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     workspace (SSWorkspace): Retained Shader Studio editors, preview, material, graph/pipeline, and
#         input/action state.
#     box (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit testing.
#
# Returns:
#     false when box.width <= 0.0 || box.height <= 0.0. Otherwise returns the completed
#     action/predicate result.
fn ss_workspace_drawer(app :: vraylib.App, workspace :: SSWorkspace, box :: vraylib.Rectangle) -> Bool {
    if box.width <= 0.0 || box.height <= 0.0 {
        return false;
    }
    clip :: vraylib.Clip := app.clip(box);
    se_clip_start(clip, box);
    try {
        vraylib.draw_rectangle_rec(box, ss_color(1));
        labels := ["Diagnostics", "Build log", "Pixels", "Pass output", "Source view"];
        i :: Int64 := 0;
        while i < 5 {
            if ss_tab(
                labels[i],
                se_rect(box.x + ss_scale(float64(i) * 136.0), box.y, ss_scale(136.0), ss_scale(30.0)),
                workspace.layout.state[3] == i
            ) {
                if workspace.panel.state[4] != 0 && i < 4 {
                    ss_workspace_diagnostics(workspace, workspace.log);
                }
                workspace.layout.state[3] = i;
                workspace.layout.state[6] = 1;
            }
            i = i + 1;
        }
        y :: Float64 := box.y + ss_scale(38.0);
        if workspace.diagnostic_notice[0] > 0 {
            ss_mono(se_text_read(workspace.diagnostic_notice), box.x + ss_scale(10.0), y, ss_color(6));
            y = y + ss_scale(24.0);
        }
        if workspace.state[11] != 0 {
            ss_mono(
                "Diagnostic capacity reached: showing first 128 rows",
                box.x + ss_scale(10.0),
                y,
                ss_color(7)
            );
            y = y + ss_scale(24.0);
        }
        if workspace.layout.state[3] == 0 || workspace.layout.state[3] == 1 {
            if ss_theme_input[0] != 0 && se_hover(box) {
                workspace.state[40] = int64(vmath.clamp(
                    float64(workspace.state[40]) - vraylib.get_mouse_wheel_move() * 3.0,
                    0.0,
                    vmath.max(0.0, float64(workspace.state[10]) - 1.0)
                ));
            }
            if workspace.state[10] == 0 {
                ss_mono(
                    "Build clean / " + ss_workspace_status(workspace),
                    box.x + ss_scale(10.0),
                    y,
                    ss_color(8)
                );
            }
            i = workspace.state[40];
            while i < workspace.state[10] && y < box.y + box.height {
                base :: Int64 := i * 513;
                message :: String := se_edit_slice(workspace.messages, base, base + workspace.messages[base]);
                row :: vraylib.Rectangle := se_rect(
                    box.x + ss_scale(8.0),
                    y,
                    box.width - ss_scale(16.0),
                    ss_scale(22.0)
                );
                if se_hover(row) {
                    vraylib.draw_rectangle_rec(row, ss_color(2));
                }
                tint :: vraylib.Color := ss_color(5);
                if workspace.state[9] != 0 {
                    tint = ss_color(7);
                }
                ss_mono(message, row.x, row.y, tint);
                if ss_theme_input[0] != 0
                    && workspace.layout.state[3] == 0
                    && se_hover(row)
                    && vraylib.is_mouse_button_pressed(vraylib.MOUSE_BUTTON_LEFT)
                    && workspace.diagnostics[i * 4] > 0 {
                    ss_workspace_diagnostic_jump(workspace, i);
                }
                y = y + ss_scale(23.0);
                i = i + 1;
            }
        }
        if workspace.layout.state[3] == 2 {
            ss_mono(
                "Pixel ("
                    +string(int64(workspace.values[5]))
                    +", "
                    +string(int64(workspace.values[6]))
                    +") / linear float RGBA",
                box.x + ss_scale(10.0),
                y,
                ss_color(4)
            );
            y = y + ss_scale(26.0);
            ss_mono(
                "Live  "
                    +string(workspace.values[7])
                    +"  "
                    +string(workspace.values[8])
                    +"  "
                    +string(workspace.values[9])
                    +"  "
                    +string(workspace.values[10]),
                box.x + ss_scale(10.0),
                y,
                ss_color(5)
            );
            y = y + ss_scale(24.0);
            ss_mono(
                "Ref   "
                    +string(workspace.values[11])
                    +"  "
                    +string(workspace.values[12])
                    +"  "
                    +string(workspace.values[13])
                    +"  "
                    +string(workspace.values[14]),
                box.x + ss_scale(10.0),
                y,
                ss_color(5)
            );
            y = y + ss_scale(24.0);
            ss_mono(
                "Peak "
                    +string(workspace.values[3])
                    +" / RMS "
                    +string(workspace.values[4])
                    +" / reference t "
                    +string(workspace.values[2]),
                box.x + ss_scale(10.0),
                y,
                ss_color(6)
            );
        }
        if workspace.layout.state[3] == 3 {
            slot :: Int64 := workspace.state[30];
            p :: SSPipeline := workspace.pipeline;
            if slot >= 0 && slot < p.state[0] && p.outputs[slot] != 0 {
                target :: vraylib.RenderTarget := vraylib.RenderTarget(p.outputs[slot], app.token);
                height :: Float64 := vmath.max(0.0, box.height - ss_scale(42.0));
                target.texture().draw_region(
                    se_rect(0.0, 0.0, float64(target.width()), float64(target.height())),
                    se_rect(box.x + ss_scale(10.0), y, height, height),
                    vraylib.vec2(0.0, 0.0),
                    0.0,
                    vraylib.WHITE
                );
                ss_mono(
                    "Pass #"
                        +string(p.meta[slot * 10])
                        +" / "
                        +string(target.width())
                        +" x "
                        +string(target.height()),
                    box.x + height + ss_scale(24.0),
                    y,
                    ss_color(5)
                );
            } else {
                ss_mono(
                    "Enable pipeline and inspect a rendered pass",
                    box.x + ss_scale(10.0),
                    y,
                    ss_color(5)
                );
            }
        }
        if workspace.layout.state[3] == 4 {
            if workspace.panel.state[4] == 0 {
                ss_mono(
                    "Select an include diagnostic to view its source context",
                    box.x + ss_scale(10.0),
                    y,
                    ss_color(5)
                );
            } else {
                if ss_workspace_action(
                    workspace,
                    "Previous",
                    se_rect(box.x + ss_scale(10.0), y, ss_scale(86.0), ss_scale(26.0)),
                    workspace.state[48] > 1
                ) {
                    ss_workspace_include_view(
                        workspace,
                        se_text_read(workspace.diagnostic_open_path),
                        int64(vmath.max(1.0, float64(workspace.state[48] - 16))),
                        workspace.state[49]
                    );
                }
                if ss_workspace_action(
                    workspace,
                    "Next",
                    se_rect(box.x + ss_scale(102.0), y, ss_scale(64.0), ss_scale(26.0)),
                    workspace.state[48] + 12 < workspace.panel.state[23]
                ) {
                    ss_workspace_include_view(
                        workspace,
                        se_text_read(workspace.diagnostic_open_path),
                        workspace.state[48] + 16,
                        workspace.state[49]
                    );
                }
                if ss_workspace_action(
                    workspace,
                    "Open externally",
                    se_rect(box.x + ss_scale(172.0), y, ss_scale(144.0), ss_scale(26.0)),
                    true
                ) {
                    workspace.panel.state[10] = 1;
                }
                y = y + ss_scale(32.0);
                i = 0;
                while i < workspace.panel.state[5] && y < box.y + box.height {
                    ss_mono(
                        se_edit_slice(workspace.messages, i * 513, i * 513 + workspace.messages[i * 513]),
                        box.x + ss_scale(10.0),
                        y,
                        ss_color(4)
                    );
                    y = y + ss_scale(22.0);
                    i = i + 1;
                }
            }
        }
    } finally {
        se_clip_finish(clip);
    }
    return true;
}
