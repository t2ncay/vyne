# Historical entry point; implementation and documentation live in VMGE.
# vynec --run examples/graphics/map_load_test.vy
use "../projects/vmge/src/game/map_load_test.vy";
try { vmge_viewer(); } catch (error) { out(error); out("VMGE_ERROR live_handles=", vraylib.live_handles()); exit(1); }
