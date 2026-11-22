use "world_tools.vy";

# Integration-owned UI state: 34 pending (1 New, 2 Open, 3 Exit),
# 35 dirty confirmation, 36 existing document, 37 exit requested,
# 38 reload world options, 39 cached frame capture, 40 modal dispatch count,
# 41 approved Open (cleared on successful load or path cancellation),
# 42 cached world modal owner, 43 cached busy world frame.

# Retained scene-document workflow state and pending paths for New/Open/Exit Save/Discard/Cancel
# decisions.
#
# Fields:
#     pending_path: Retained proposed path for the pending document action.
interface SEDocument {
    pending_path :: Array<Int64>,
}

# Create a scene-document workflow owner before frames.
#
# Returns:
#     SEDocument containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn se_document() -> SEDocument {
    bytes :: Array<Int64> := [];
    bytes.reserve(256);
    bytes.resize(256, 0);
    return SEDocument(bytes);
}

# Cancel pending scene file/dialog actions and release active text-field focus without modifying the
# document.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     document (SEDocument): Retained New/Open/Exit workflow, dirty-decision state, and pending scene
#         path.
#
# Returns:
#     True when the operation completes without throwing.
fn se_document_cancel(ui :: SEUI, document :: SEDocument) -> Bool {
    ui.state[34] = 0;
    ui.state[35] = 0;
    ui.state[13] = 0;
    ui.state[1] = -1;
    ui.state[41] = 0;
    document.pending_path[0] = 0;
    return true;
}

# Complete an approved New/Open/Exit action after any required save succeeds.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     history (SEHistory): Preallocated undo/redo snapshots and saved-revision tracking.
#     path (Array<Int64>): Retained length-prefixed UTF-8 path buffer.
#     document (SEDocument): Retained New/Open/Exit workflow, dirty-decision state, and pending scene
#         path.
#
# Returns:
#     True when the operation completes without throwing.
fn se_document_finish(
    ui :: SEUI,
    scene :: SEScene,
    history :: SEHistory,
    path :: Array<Int64>,
    document :: SEDocument
) -> Bool {
    action :: Int64 := ui.state[34];
    ui.state[35] = 0;
    if action == 1 {
        se_clear(scene);
        ui.state[0] = scene.spawn(0, "Scene", -1);
        ui.state[2] = -1;
        ui.state[6] = -1;
        ui.state[7] = 0;
        se_history_reset(history, scene, false);
        ui.state[36] = 0;
        ui.state[38] = 1;
    }
    if action == 2 {
        value :: String := se_text_read(document.pending_path);
        if value == "" {
            value = se_text_read(path);
        }
        se_edit_begin(ui, 16, -1, value);
        ui.state[13] = 1;
        ui.state[41] = 1;
    }
    if action == 3 {
        ui.state[37] = 1;
    }
    ui.state[34] = 0;
    document.pending_path[0] = 0;
    return true;
}

# Request New/Open/Exit, prompting for dirty-scene decisions or immediately finishing a clean action.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     history (SEHistory): Preallocated undo/redo snapshots and saved-revision tracking.
#     path (Array<Int64>): Retained length-prefixed UTF-8 path buffer.
#     document (SEDocument): Retained New/Open/Exit workflow, dirty-decision state, and pending scene
#         path.
#     action (Int64): Document request: 1 New, 2 Open, 3 Exit.
#     value (String): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     false when ui.state[34] != 0. Otherwise returns the completed action/predicate result.
fn se_document_request(
    ui :: SEUI,
    scene :: SEScene,
    history :: SEHistory,
    path :: Array<Int64>,
    document :: SEDocument,
    action :: Int64,
    value :: String
) -> Bool {
    if ui.state[34] != 0 {
        return false;
    }
    se_text_write(document.pending_path, value);
    ui.state[34] = action;
    ui.state[1] = -1;
    ui.state[13] = 0;
    if history.dirty() {
        ui.state[35] = 1;
        return true;
    }
    return se_document_finish(ui, scene, history, path, document);
}

# Save the current scene or open Save As, retaining pending actions and dirty state if the write
# fails.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     history (SEHistory): Preallocated undo/redo snapshots and saved-revision tracking.
#     path (Array<Int64>): Retained length-prefixed UTF-8 path buffer.
#     document (SEDocument): Retained New/Open/Exit workflow, dirty-decision state, and pending scene
#         path.
#     save_as (Bool): Whether to choose a new scene path instead of saving the current identity.
#
# Returns:
#     True when the operation completes without throwing.
fn se_document_save(
    ui :: SEUI,
    scene :: SEScene,
    history :: SEHistory,
    path :: Array<Int64>,
    document :: SEDocument,
    save_as :: Bool
) -> Bool {
    if save_as || ui.state[36] == 0 {
        se_edit_begin(ui, 16, -1, se_text_read(path));
        ui.state[13] = 2;
        ui.state[35] = 0;
        return true;
    }
    # Save and validate before publishing the clean marker or pending action.
    se_world_save_scene(scene, se_text_read(path));
    history.saved();
    if ui.state[34] != 0 {
        se_document_finish(ui, scene, history, path, document);
    }
    return true;
}

# Dispatch Save/Discard/Cancel for the pending scene action without losing unsaved state on failed
# saves.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     history (SEHistory): Preallocated undo/redo snapshots and saved-revision tracking.
#     path (Array<Int64>): Retained length-prefixed UTF-8 path buffer.
#     document (SEDocument): Retained New/Open/Exit workflow, dirty-decision state, and pending scene
#         path.
#     choice (Int64): Dialog decision: 0 Cancel, 1 Save, 2 Discard.
#
# Returns:
#     false when ui.state[35] == 0. Otherwise returns the completed action/predicate result.
fn se_document_choice(
    ui :: SEUI,
    scene :: SEScene,
    history :: SEHistory,
    path :: Array<Int64>,
    document :: SEDocument,
    choice :: Int64
) -> Bool {
    if ui.state[35] == 0 {
        return false;
    }
    if choice == 0 {
        return se_document_cancel(ui, document);
    }
    if choice == 1 {
        return se_document_save(ui, scene, history, path, document, false);
    }
    return se_document_finish(ui, scene, history, path, document);
}

# Draw the dirty-document decision dialog and consume its modal mouse/keyboard choices.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     scene (SEScene): Scene nodes, retained names/paths, material instances, environment settings,
#         and world resource.
#     history (SEHistory): Preallocated undo/redo snapshots and saved-revision tracking.
#     path (Array<Int64>): Retained length-prefixed UTF-8 path buffer.
#     document (SEDocument): Retained New/Open/Exit workflow, dirty-decision state, and pending scene
#         path.
#
# Returns:
#     false when ui.state[35] == 0. Otherwise returns the completed action/predicate result.
fn se_document_dialog(
    app :: vraylib.App,
    ui :: SEUI,
    scene :: SEScene,
    history :: SEHistory,
    path :: Array<Int64>,
    document :: SEDocument
) -> Bool {
    if ui.state[35] == 0 {
        return false;
    }
    vraylib.draw_rectangle(0, 0, app.width(), app.height(), vraylib.rgba(0, 0, 0, 160));
    x :: Float64 := vmath.max(8.0, (float64(app.width()) - 420.0) * 0.5);
    y :: Float64 := float64(app.height()) * 0.35;
    se_panel(se_rect(x, y, 404.0, 128.0), "Unsaved scene changes");
    se_draw_text("Save changes before continuing?", int64(x + 12.0), int64(y + 39.0), 16, vraylib.WHITE);
    if se_button("Save", se_rect(x + 12.0, y + 79.0, 116.0, 30.0), true) {
        se_document_choice(ui, scene, history, path, document, 1);
    }
    if se_button("Discard", se_rect(x + 140.0, y + 79.0, 116.0, 30.0), true) {
        se_document_choice(ui, scene, history, path, document, 2);
    }
    if se_button("Cancel", se_rect(x + 268.0, y + 79.0, 116.0, 30.0), true) {
        se_document_choice(ui, scene, history, path, document, 0);
    }
    return true;
}

# Cancel focused scene/world/document editors and clear their modal ownership state.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     tools (SEWorldTools): Retained brush options, before-image/preview owner, and world settings
#         modal.
#     document (SEDocument): Retained New/Open/Exit workflow, dirty-decision state, and pending scene
#         path.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn se_editor_cancel_focus(ui :: SEUI, tools :: SEWorldTools, document :: SEDocument) -> Bool {
    if ui.state[42] != 0 || se_world_tools_modal_open(tools) {
        se_world_settings_cancel(tools.settings);
        ui.state[39] = 1;
        return true;
    }
    if ui.state[35] != 0 || ui.state[13] != 0 || ui.state[1] != -1 || ui.state[39] != 0 {
        se_document_cancel(ui, document);
        ui.state[39] = 1;
        return true;
    }
    return false;
}

# Dispatch focused world/document/scene input once and claim the entire modal frame before global
# shortcuts.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     tools (SEWorldTools): Retained brush options, before-image/preview owner, and world settings
#         modal.
#     document (SEDocument): Retained New/Open/Exit workflow, dirty-decision state, and pending scene
#         path.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn se_editor_input(ui :: SEUI, tools :: SEWorldTools, document :: SEDocument) -> Bool {
    # Cache ownership before dispatch. Closing a modal never releases this frame.
    ui.state[39] = 0;
    ui.state[42] = 0;
    ui.state[43] = 0;
    if ui.state[47] != 0 {
        ui.state[39] = 1;
        se_node_picker_input(ui);
        return false;
    }
    if se_world_tools_busy(tools) {
        ui.state[43] = 1;
    }
    if se_world_tools_modal_open(tools) {
        ui.state[39] = 1;
        ui.state[42] = 1;
        ui.state[40] = ui.state[40] + 1;
        se_world_tools_modal_input(tools);
        if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) {
            se_editor_cancel_focus(ui, tools, document);
        }
        return false;
    }
    if ui.state[35] != 0 {
        ui.state[39] = 1;
        if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) {
            se_editor_cancel_focus(ui, tools, document);
        }
        return false;
    }
    dialog :: Bool := ui.state[13] != 0;
    if dialog || ui.state[1] != -1 {
        ui.state[39] = 1;
    }
    if se_world_tools_busy(tools) {
        return false;
    }
    result :: Bool := se_edit_input(ui);
    if vraylib.is_key_pressed(vraylib.KEY_ESCAPE) {
        se_editor_cancel_focus(ui, tools, document);
    }
    if dialog && ui.state[13] == 0 {
        ui.state[41] = 0;
    }
    if ui.state[34] != 0 && ui.state[13] == 0 && ui.state[1] == -1 {
        se_document_cancel(ui, document);
    }
    return result;
}

# Reload world-tool options and dependent UI caches after scene replacement/import flags.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     tools (SEWorldTools): Retained brush options, before-image/preview owner, and world settings
#         modal.
#     world (SEWorld): Fixed scene-owned cube/terrain payload and reusable numeric/drawing scratch.
#
# Returns:
#     false when ui.state[38] == 0. Otherwise returns the completed action/predicate result.
fn se_editor_reload(ui :: SEUI, tools :: SEWorldTools, world :: SEWorld) -> Bool {
    if ui.state[38] == 0 {
        return false;
    }
    se_world_settings_cancel(tools.settings);
    tools.state[5] = 0;
    tools.state[6] = -2;
    # Reset generator defaults as well when loading a node-only or legacy scene.
    tools.numbers[21] = 0.06;
    tools.numbers[22] = 4.0;
    tools.numbers[23] = 0.5;
    tools.numbers[25] = 3.0;
    se_world_tools_load_options(tools, world);
    ui.state[38] = 0;
    return true;
}

# Create a passive UI view sharing retained storage for background drawing without accepting active
# input.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#
# Returns:
#     SEUI containing the resulting retained state or payload.
fn se_editor_passive(ui :: SEUI) -> SEUI {
    state :: Array<Int64> := [];
    state.reserve(64);
    state.resize(64, 0);
    numbers :: Array<Float64> := [];
    numbers.reserve(32);
    numbers.resize(32, 0.0);
    return SEUI(
        state,
        numbers,
        ui.text,
        ui.edit_buffer,
        ui.asset_names,
        ui.asset_kinds,
        ui.asset_directory,
        ui.asset_search,
        ui.tree_search,
        ui.collapsed,
        ui.preview_tokens,
        ui.preview_paths,
        ui.browser_state,
        ui.external_path,
        ui.asset_notice,
        ui.view_state,
        ui.view_values
    );
}

# Copy numeric active UI state into the passive view used while a modal owns the frame.
#
# Parameters:
#     ui (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#     passive (SEUI): Retained scene UI state, focus/caret/edit buffers, browser, hierarchy, and view
#         controls.
#
# Returns:
#     True when the operation completes without throwing.
fn se_editor_freeze(ui :: SEUI, passive :: SEUI) -> Bool {
    i :: Int64 := 0;
    while i < 64 {
        passive.state[i] = ui.state[i];
        i = i + 1;
    }
    i = 0;
    while i < 32 {
        passive.numbers[i] = ui.numbers[i];
        i = i + 1;
    }
    passive.state[13] = -1;
    passive.state[32] = 0;
    return true;
}
