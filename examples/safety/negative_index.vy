# SAFETY: negative scratch index.
# Expected: compiles, runs, aborts with VNE-072.

region r {
    scratch buf :: Float64[8];
    i :: Int64 := -1;
    buf[i] = 1.0;
    out("negative_index: reached end (BUG)");
};
