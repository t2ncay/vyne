# contract_evidence_test.vy — evidence mode writes a machine-readable log.
#
#   ./vynec --compile examples/test/contract_evidence_test.vy
#   ./examples/test/contract_evidence_test
#
# The runtime writes CSV lines to vyne_contracts.log (or
# $VYNE_CONTRACT_LOG) when contracts = "evidence". This test reads the
# log back through vfs and asserts that both clauses recorded a PASS.

use native vfs;

ruleset { contracts = "evidence" };

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

fn positive(x :: Int64) -> Int64
requires(x > 0)
ensures(result > 0)
{
    return x * 2;
}

out("=== contracts evidence ===");

v :: Int64 := positive(5);
check("evidence-value", v == 10);

log :: String := vfs.read("vyne_contracts.log");
check("evidence-requires", log.find("PASS,requires,positive") >= 0);
check("evidence-ensures", log.find("PASS,ensures,positive") >= 0);

if failures[0] > 0 {
    out("contracts-evidence: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("contracts-evidence: all checks passed");
