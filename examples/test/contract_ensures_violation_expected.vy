# contract_ensures_violation_expected.vy — expected to FAIL with VNE-111.
#
#   ./vynec --compile examples/test/contract_ensures_violation_expected.vy
#   ./examples/test/contract_ensures_violation_expected
#
# `broken` returns a value that violates its own `ensures(result > 0)`.

ruleset { contracts = "check" };

fn broken(x :: Int64) -> Int64
ensures(result > 0)
{
    return -1;
}

out("=== ensures violation (expected failure) ===");
v :: Int64 := broken(1);
out("unreachable");
exit(2);
