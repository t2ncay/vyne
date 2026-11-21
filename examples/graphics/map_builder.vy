# Historical entry point; implementation and documentation live in VMGE.
# vynec --run examples/graphics/map_builder.vy
use "../projects/vmge/src/editor/map_builder.vy";
try { vmge_editor(); } catch (error) { out(error); out("VMGE_ERROR live_handles=", vraylib.live_handles()); exit(1); }
