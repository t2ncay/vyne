# Numerical cases chosen to detect premature narrowing and lost rounding points.
use native vmath;
events :: Array<Int64> := [0];
fn next_value() -> Float64 {
    events[0] = events[0] + 1;
    return float64(events[0]) + 0.25;
}
wide :: Float64 := 16777216.0;
unit :: Float64 := 1.0;
late :: Float32 := float32((wide + unit) - wide);
early :: Float64 := float64(float32(wide + unit)) - wide;
if late != 1.0f || early != 0.0 { exit(1); }
small :: Float32 := float32(wide);
rounded :: Float32 := float32(small + float32(unit));
if rounded != 16777216.0f { exit(2); }
mixed :: Float64 := float32(1.5) + float64(2.25);
if mixed != 3.75 || type(mixed) != "Float64" { exit(3); }
if type(float32(1.5) + float32(2.25)) != "Float32" { exit(4); }
if float32("1.5") != 1.5f || float64("-2.25") != -2.25 { exit(5); }
# Preserve the generated runtime's existing boolean conversion defaults.
if float32(true) != 0.0f || float64(true) != 0.0 { exit(6); }
ordered :: Float32 := float32(next_value()) + float32(next_value());
if ordered != 3.5f || events[0] != 2 { exit(7); }
nested :: Float64 := float64(float32(next_value()));
if nested != 3.25 || events[0] != 3 { exit(8); }
if string(float32(-0.0)) != "-0.0" { exit(9); }
large :: Float32 := float32(1e39);
if large <= float32(1e38) { exit(10); }
nan_value :: Float32 := float32(vmath.sqrt(-1.0));
if nan_value == nan_value { exit(11); }
region scratch_check {
    scratch input :: Float32[2];
    input[0] = float32(1.5);
    input[1] = float32(2.25);
    total :: Float32 := float32(float32(input[0] * input[1]) + float32(0.25));
    if total != 3.625f { exit(12); }
};
out("NATIVE_CONVERSIONS_PASS");
