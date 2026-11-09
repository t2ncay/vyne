# realtime_test.vy — @realtime<ms> deadline policy and telemetry.
#
#   ./vynec --compile examples/test/realtime_test.vy
#   ./examples/test/realtime_test

use native vcore;

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

fn spin_ms(ms :: Int64) -> Int64 {
    start :: Float64 := float64(vcore.now_ns());
    limit :: Float64 := float64(ms) * 1000000.0;
    while float64(vcore.now_ns()) - start < limit {
        # busy wait
    }
    return 0;
}

out("=== @realtime ===");

# A region with a generous deadline records its worst observed time.
@realtime<2000> region fast {
    spin_ms(2);
};

check("fast-overruns", vcore.realtime_overruns() == 0);
check("fast-worst-nonneg", vcore.realtime_worst_ms() >= 0.0);
check("fast-named-worst", vcore.realtime_worst_ms_named("fast") >= 0.0);
check("fast-named-overruns", vcore.realtime_overruns_named("fast") == 0);

# An early return must still run the deadline check and unwind the region.
fn early() -> Int64 {
    @realtime<2000> region inner {
        return 7;
    };
    return 0;
}

check("early-return", early() == 7);
check("early-return-overruns", vcore.realtime_overruns_named("inner") == 0);

# Nested realtime regions both check.
@realtime<2000> region outer {
    @realtime<2000> region nested {
        spin_ms(1);
    };
};

check("nested-overruns", vcore.realtime_overruns() == 0);

# Reset clears the aggregate counters.
vcore.realtime_reset();
check("reset-worst", vcore.realtime_worst_ms() == 0.0);
check("reset-overruns", vcore.realtime_overruns() == 0);

if failures[0] > 0 {
    out("realtime: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("realtime: all checks passed");
