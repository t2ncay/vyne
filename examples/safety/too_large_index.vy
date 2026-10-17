# SAFETY: index equal to array length.
# Expected: compiles, runs, aborts with VNE-072.

region r {
    scratch buf :: Float64[8];
    i :: Int64 := 8;
    buf[i] = 1.0;
    out("too_large_index: reached end (BUG)");
};
