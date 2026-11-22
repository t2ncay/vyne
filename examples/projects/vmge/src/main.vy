# From the VMGE directory: vynec build / vynec run. Scene defaults to the shared studio.
use "editor/map_builder.vy";
use "game/map_load_test.vy";
use "editor/scene_editor.vy";

try {
    mode :: String := vmge_option("VYNE_VMGE_MODE", "scene");
    if mode == "scene" {
        se_scene_editor();
    } else {
        if mode == "shader" {
            se_scene_editor_workspace(2);
        } else {
            if mode == "rendering" {
                se_scene_editor_workspace(3);
            } else {
                if mode == "world" {
                    se_scene_editor_workspace(1);
                } else {
                    if mode == "editor" {
                        vmge_editor();
                    } else {
                        if mode == "play" {
                            vmge_viewer();
                        } else {
                            throw "VYNE_VMGE_MODE must be scene, world, shader, rendering, editor, or play";
                        }
                    }
                }
            }
        }
    }
} catch (error) {
    out(error);
    out("VMGE_ERROR live_handles=", vraylib.live_handles());
    exit(1);
}
