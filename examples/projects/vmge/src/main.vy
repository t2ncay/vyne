# From the VMGE directory: vynec build / vynec run. VYNE_VMGE_MODE selects editor or play.
use "editor/map_builder.vy";
use "game/map_load_test.vy";

try {
    mode :: String := vmge_option("VYNE_VMGE_MODE", "editor");
    if mode == "editor" { vmge_editor(); }
    else {
        if mode == "play" { vmge_viewer(); }
        else { throw "VYNE_VMGE_MODE must be editor or play"; }
    }
} catch (error) { out(error); out("VMGE_ERROR live_handles=", vraylib.live_handles()); exit(1); }
