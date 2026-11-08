# codegen_peepholes.vy — identity folding and literal-divisor checks.
#
# Run compiled:
#   ./vynec --compile examples/test/codegen_peepholes.vy
#   ./examples/test/codegen_peepholes
#
# The values below are deliberately written so the emitter can fold them:
#   x + 0, 0 + x, x - 0, x * 1, 1 * x, x / 1
# and so division/modulo by a non-zero literal can skip the runtime
# divide-by-zero check. Results must be unchanged.

out("=== codegen peepholes ===");

x :: Int64 := 10;

a :: Int64 := x + 0;
b :: Int64 := 0 + x;
c :: Int64 := x - 0;
d :: Int64 := x * 1;
e :: Int64 := 1 * x;
f :: Int64 := x / 1;

out(a == 10);
out(b == 10);
out(c == 10);
out(d == 10);
out(e == 10);
out(f == 10);
out(a + b + c + d + e + f == 60);

p :: Float64 := 2.5;

q :: Float64 := p - 0.0;
r :: Float64 := p * 1.0;
s :: Float64 := p / 1.0;

out(q == 2.5);
out(r == 2.5);
out(s == 2.5);
out(q + r + s == 7.5);

# Literal, non-zero divisors: the emitted C has no zero check.
out(100 / 4 == 25);
out(100 % 7 == 2);
out(100.0 / 4.0 == 25.0);

out("=== done ===");
