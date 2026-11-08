# vcsv_bench.vy - CSV parse and serialize throughput.
#
# Run compiled:
#   ./vynec --compile examples/benchmark/vcsv_bench.vy
#   ./examples/benchmark/vcsv_bench

use external "vcsv/vcsv.vy";
use native vcore;

out("vcsv benchmark");

N :: Int64 := 10000;
sb :: Int64 := vcore.sb_create();
vcore.sb_append(sb, "a,b,c,d,e\n");
through i :: 0..N-1 -> loop {
    vcore.sb_append(sb, i);
    vcore.sb_append(sb, ",");
    vcore.sb_append(sb, i * 2);
    vcore.sb_append(sb, ",");
    vcore.sb_append(sb, i * 3);
    vcore.sb_append(sb, ",");
    vcore.sb_append(sb, i * 4);
    vcore.sb_append(sb, ",");
    vcore.sb_append(sb, i * 5);
    vcore.sb_append(sb, "\n");
};
text :: String := vcore.sb_build(sb);
out("input bytes: " + string(sizeof(text)));

t0 :: Int64 := vcore.now_ns();
t :: vcsv.Table := vcsv.parse(text, true);
t1 :: Int64 := vcore.now_ns();
out("parse      : " + string((t1 - t0) / 1000000) + " ms (" + string(t.n_rows) + " rows)");

t2 :: Int64 := vcore.now_ns();
out_text :: String := vcsv.serialize(t);
t3 :: Int64 := vcore.now_ns();
out("serialize  : " + string((t3 - t2) / 1000000) + " ms (" + string(sizeof(out_text)) + " bytes)");
