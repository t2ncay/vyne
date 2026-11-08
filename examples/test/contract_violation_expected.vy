# contract_violation_expected.vy — expected to FAIL with VNE-111.
#
#   ./vynec --compile examples/test/contract_violation_expected.vy
#   ./examples/test/contract_violation_expected    # exit 1, prints VNE-111
#
# The call `positive(0)` violates `requires(x > 0)`.

ruleset { contracts = "check" };

fn positive(x :: Int64) -> Int64
requires(x > 0)
ensures(result > 0)
{
    return x * 2;
}

out("=== requires violation (expected failure) ===");
v :: Int64 := positive(0);
out("unreachable");
exit(2);
