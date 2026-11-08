# realtime_overrun_expected.vy — expected to FAIL with VNE-109.
#
#   ./vynec --compile examples/test/realtime_overrun_expected.vy
#   ./examples/test/realtime_overrun_expected    # exit 1, prints VNE-109
#
# This file is a negative test: the @realtime<1> region deliberately
# exceeds its 1 ms deadline. The panic is the expected behaviour.

use native vcore;

fn spin_ms(ms :: Int64) -> Int64 {
    start :: Float64 := float64(vcore.now_ns());
    limit :: Float64 := float64(ms) * 1000000.0;
    while float64(vcore.now_ns()) - start < limit {
        # busy wait
    }
    return 0;
}

out("=== @realtime overrun (expected failure) ===");

@realtime<1> region too_slow {
    spin_ms(10);
};

out("unreachable: the deadline check should have fired");
exit(2);
