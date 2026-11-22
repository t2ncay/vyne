use "../src/ui/workspace_layout.vy";
fn layout_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
layout :: SSLayout := ss_layout();
bounds :: vraylib.Rectangle := se_rect(10.0,20.0,1280.0,800.0);
i :: Int64 := 0;
while i < 10 {
    box :: vraylib.Rectangle := ss_layout_region(layout,bounds,i);
    layout_check(box.width >= 0.0 && box.height >= 0.0 && box.x >= bounds.x && box.y >= bounds.y && box.x+box.width <= bounds.x+bounds.width+0.001 && box.y+box.height <= bounds.y+bounds.height+0.001,"layout region stays in host bounds"); i = i+1;
}
layout.state[0] = 3; layout.state[1] = 1; layout.state[4] = 1; layout.values[0] = 0.7; layout.values[3] = 2.0;
copy :: SSLayout := ss_layout(); layout_check(ss_layout_decode(copy,ss_layout_encode(layout)),"layout roundtrip parses");
layout_check(copy.state[0] == 3 && copy.state[1] == 1 && copy.state[4] == 1 && copy.values[0] == 0.7 && copy.values[3] == 2.0,"saved tabs splits DPI and maximization roundtrip");
layout_check(ss_layout_region(copy,bounds,2).width == 0.0 && ss_layout_region(copy,bounds,3).width == bounds.width,"maximized preview owns content");
layout_check(!ss_layout_decode(copy,"VMGE_LAYOUT|1\n9|0|0|0|0\n0.6|280|164|1\n") && copy.state[0] == 3,"invalid layout is atomic");
layout_check(!ss_layout_decode(copy,"VMGE_LAYOUT|1\nx|0|0|0|0\n0.6|280|164|1\n") && copy.state[0] == 3,"malformed numeric token rejected");
layout_check(vfs.is_dir("build"),"unique layout fixture parent exists");
layout_check(ss_layout_save(copy,"build/shader-ui-dev-roundtrip.layout"),"layout saved atomically");
layout_check(copy.state[6] == 0,"successful write clears dirty layout");
layout_check(!ss_layout_save(copy,"build/shader-ui-dev-no-layout-parent/layout") && copy.state[6] == 1,"failed atomic write keeps pending dirty layout");
layout_check(ss_layout_save(copy,"build/shader-ui-dev-roundtrip.layout") && copy.state[6] == 0,"retry clears dirty flag only after successful write");
disk :: SSLayout := ss_layout(); layout_check(ss_layout_load(disk,"build/shader-ui-dev-roundtrip.layout") && disk.state[0] == 3 && disk.values[3] == 2.0,"saved layout reloads");
copy.state[4] = 0; bounds = se_rect(0.0,0.0,64.0,32.0); i = 0;
while i < 10 { box :: vraylib.Rectangle := ss_layout_region(copy,bounds,i); layout_check(box.width >= 0.0 && box.height >= 0.0,"tiny host has nonnegative regions"); i = i+1; }
out("VMGE_LAYOUT_OK");
