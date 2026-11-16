# C03: current native dispatch emits invalid scratch .data access.
# Retained reproducer; not a passing V02 detector test. See issue #170.
fn read_probe(values :: Array<Float64>, index :: Int64) -> Float64 {
    return values[index];
}
region probe {
    scratch values :: Float64[4];
    values[0] = 1.0;
    if read_probe(values, 0) != 1.0 { exit(9); }
    out("C03 scratch array argument: PASS");
};
