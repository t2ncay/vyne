use "../src/engine/export.vy";

fn vmge_check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
vfs.mkdir_p("build/vmge-tests");
world :: VMGEMap := vmge_map_parse(" -2,1,4,2,0;\n1e1,3,-6,4,7; -2,1,4,2,0;", 8);
vmge_check(world.count() == 3 && world.value(1, 0) == 10.0, "load legacy records, whitespace and exponents");
vmge_check(!world.place(-2.0, 1.0, 4.0, 2.0, 1), "reject occupied cell");
vmge_check(world.remove(-2.0, 1.0, 4.0) == 2 && world.count() == 1, "remove every legacy duplicate");
vmge_check(world.place(2.0, 1.0, -4.0, 2.0, 2), "place into reused capacity");
copy :: VMGEMap := vmge_map_parse(vmge_map_text(world), 8);
vmge_check(copy.count() == world.count(), "serialize record count");
precise :: VMGEMap := vmge_map_parse("0.1234567890123,-0,1.000000000001,0.3333333333333,2;", 8);
precise_copy :: VMGEMap := vmge_map_parse(vmge_map_text(precise), 8);
vmge_check(precise.value(0, 0) == precise_copy.value(0, 0) && precise.value(0, 3) == precise_copy.value(0, 3), "legacy floating point precision survives save/reload");
i :: Int64 := 0;
while i < world.count() {
    field :: Int64 := 0;
    while field < 5 { vmge_check(world.value(i, field) == copy.value(i, field), "map numeric round trip"); field = field + 1; }
    i = i + 1;
}
bad := ["0,0,0,2;", "0,,0,2,0;", "0,0,0,0,0;", "0,0,0,-1,0;", "0,0,0,2,8;", "0,0,0,2,-1;", "0,0,0,2,1.5;", "oops,0,0,2,0;", "1e,0,0,2,0;", "1e309,0,0,2,0;", "NaN,0,0,2,0;", "0,0,0,2,0,1;"];
through text :: bad -> loop {
    failed :: Bool := false;
    try { vmge_map_parse(text, 8); } catch (error) { failed = true; }
    vmge_check(failed, "reject malformed record: " + text);
};
full :: VMGEMap := vmge_map_create(1);
full.append(0.0, 1.0, 0.0, 2.0, 0);
failed :: Bool := false;
try { full.append(2.0, 1.0, 0.0, 2.0, 1); } catch (error) { failed = true; }
vmge_check(failed && full.count() == 1, "capacity failure preserves map");
fixture :: String := "build/vmge-tests/roundtrip.dat";
vmge_map_save(world, fixture);
disk :: VMGEMap := vmge_map_load(fixture, 8, false);
vmge_check(vmge_map_text(disk) == vmge_map_text(world), "filesystem round trip");
vmge_map_export(world, "build/vmge-tests/roundtrip.obj");
vmge_check(vmge_map_load("build/vmge-tests/absent.dat", 8, true).count() == 0, "editor starts empty");
failed = false;
try { vmge_map_load("build/vmge-tests/absent.dat", 8, false); } catch (error) { failed = true; }
vmge_check(failed, "viewer requires explicit map");
failed = false;
try { vmge_map_save(world, "build/vmge-tests/absent-parent/map.dat"); } catch (error) { failed = true; }
vmge_check(failed, "write failures are reported");
out("VMGE_MAP_OK");
