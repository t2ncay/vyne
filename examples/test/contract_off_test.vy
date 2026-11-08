# contract_off_test.vy — `ruleset { contracts = "off" }` emits no checks.
#
#   ./vynec --compile examples/test/contract_off_test.vy
#   ./examples/test/contract_off_test
#
# Both functions below would fail their contracts under "check" mode.
# In "off" mode the calls run to completion, which is the release path.

ruleset { contracts = "off" };

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

fn broken(x :: Int64) -> Int64
ensures(result > 0)
{
    return -1;
}

out("=== contracts off ===");
check("off-skips-requires", positive(-1) == -2);
check("off-skips-ensures", broken(1) == -1);

if failures[0] > 0 {
    out("contracts-off: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("contracts-off: all checks passed");
