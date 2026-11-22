# From the VMGE directory: vynec build / vynec run. Modes: scene (default), editor, play.
use "editor/map_builder.vy";
use "game/map_load_test.vy";
use "editor/scene_editor.vy";

try {
    mode :: String := vmge_option("VYNE_VMGE_MODE", "scene");
    if mode == "scene" { se_scene_editor(); }
    else {
        if mode == "editor" { vmge_editor(); }
        else {
            if mode == "play" { vmge_viewer(); }
            else { throw "VYNE_VMGE_MODE must be scene, editor, or play"; }
        }
    }
} catch (error) { out(error); out("VMGE_ERROR live_handles=", vraylib.live_handles()); exit(1); }
