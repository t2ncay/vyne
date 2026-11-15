# V02: deliberate stack overflow, only run with --no-scratch-bounds and ASan.
# The profile itself must never disable the language's bounds checks.
region probe {
    scratch values :: Float64[4];
    values[0] = 1.0;
    index :: Int64 := 4;
    out("V02 ASan probe entered");
    out(values[index]);
    out("V02 ASan probe escaped (BUG)");
};
