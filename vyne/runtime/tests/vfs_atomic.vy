use native vfs;
ruleset { dynamic_casting };
fn check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
use native vraylib;
path :: String := vraylib.environment("VYNE_ATOMIC_TEST_PATH");
check(vfs.write(path, "original"), "original write");
check(vfs.write_atomic(path, "replacement Azərbaycan\n"), "atomic replacement");
check(vfs.read(path) == "replacement Azərbaycan\n", "atomic text roundtrip");
check(vfs.write_atomic(path, ""), "empty atomic replacement");
check(vfs.size(path) == 0, "empty replacement size");
vfs.remove(path);
check(vfs.write_atomic(path, "created"), "atomic creation");
check(vfs.read(path) == "created", "atomic creation content");
vfs.remove(path);
out("VFS_ATOMIC_VY_OK");
