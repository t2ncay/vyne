# V01: a real generated-runtime failure must reach the calling process.
region probe {
    scratch values :: Float64[4];
    index :: Int64 := 4;
    values[index] = 1.0;
    out("V01 unreachable (BUG)");
};
