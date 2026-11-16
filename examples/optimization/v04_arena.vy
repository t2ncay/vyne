# V04: explicitly report a post-rewind snapshot, separate from process memory.
use native vmem;
use external "vlin/vlin.vy";

vmem.peak_reset();
before :: Int64 := vmem.total_allocated();
region probe {
    values :: Array<Float64> := vlin.zeros_f64(1024);
    values[0] = 7.0;
    if values[0] != 7.0 { exit(9); }
    if vmem.total_allocated() < before + 8192 { exit(10); }
};
live :: Int64 := vmem.total_allocated();
peak :: Int64 := vmem.peak_allocated();
commit_live :: Int64 := vmem.commit_total_allocated();
commit_peak :: Int64 := vmem.commit_peak_allocated();
if live != before { exit(11); }
if peak < before + 8192 { exit(12); }
# dump() reports counters and actual block capacity without arena allocation.
vmem.dump();
out("checksum: 7");
