# contract_test.vy — requires/ensures/@invariant in "check" mode.
#
#   ./vynec --compile examples/test/contract_test.vy
#   ./examples/test/contract_test

ruleset { contracts = "check" };

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

fn ratio(a :: Int64, b :: Int64) -> Int64
requires(b != 0)
ensures(result * b == a)
{
    return a / b;
}

fn factorial(n :: Int64) -> Int64
requires(n >= 0)
ensures(result >= 1)
{
    total :: Int64 := 1;
    through i :: 1..n -> loop {
        total = total * i;
    };
    return total;
}

out("=== contracts ===");

check("requires-ensures-positive", positive(5) == 10);
check("requires-ensures-ratio", ratio(10, 2) == 5);
check("requires-ensures-factorial", factorial(5) == 120);

total :: Int64 := 0;
@invariant(i >= 0)
through i :: 0..9 -> loop {
    total = total + i;
};
check("invariant-loop", total == 45);

if failures[0] > 0 {
    out("contracts: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("contracts: all checks passed");
