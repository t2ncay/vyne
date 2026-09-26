# region_test.vy
#
# A1 lexical-region smoke test.
#
# The point of the test is that the arena level *returns to the
# pre-region baseline* every iteration. For that to be visible, the
# baseline itself has to be non-zero — otherwise "flat at 0" looks
# the same as "total_allocated() is broken."
#
# So: allocate a persistent keeper array first, note the counter,
# then verify the region loop rewinds back to that same number.

ruleset { dynamic_casting };
module vmem;

# --- 1. persistent allocation, outside any region -----------------
# This value survives for the whole program. It bumps the arena and
# establishes the non-zero baseline the loop will return to.
keeper :: Array<Int64> = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];

out("after keeper:");
out(vmem.total_allocated());

# --- 2. region loop ------------------------------------------------
# Everything below allocates inside `scratch` and must be reclaimed
# at the closing brace. If the rewind fires correctly, every
# printed number matches the "after keeper" number above.
out("arena bytes after each region:");

through epoch :: 0..5 -> loop {
    region scratch {
        tmp :: Array<Int64> = [100, 200, 300, 400, 500, 600, 700, 800];
        acc :: Int64 = 0;
        through x :: tmp -> loop {
            acc = acc + x;
        };
        # A second, nested allocation to prove the arena keeps growing
        # inside the region regardless of how much we throw at it.
        more :: Array<Int64> = [1000, 2000, 3000, 4000];
        through y :: more -> loop {
            acc = acc + y;
        };
    };
    out(vmem.total_allocated());
};

# --- 3. sanity check -----------------------------------------------
# The keeper must still be readable and correct after six rewinds.
# If a buggy rewind walked past the checkpoint and dropped keeper's
# block, this would either crash or print garbage.
out("keeper after loop:");
out(keeper);

out("final arena bytes:");
out(vmem.total_allocated());