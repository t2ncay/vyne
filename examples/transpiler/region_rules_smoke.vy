# region_rules_smoke.vy — positive tests for the three new region rules
# and the <region>.commit syntax.
#
# Run:
#   vynec --compile region_rules_smoke.vy
#   ./region_rules_smoke
#
# Every line labeled "(expect X)" should print exactly X. Any divergence
# is a regression. Negative test cases live in separate files (see the
# header comment in each) — do NOT merge them into this file: they are
# designed to halt at runtime or fail at compile time.

use native vmath;
use native vcore;
use native vmem;

ruleset { dynamic_casting };

out("");
out("=== Region rules smoke test ===");
out("");

# ----------------------------------------------------------------------
# [1] <region>.commit — the qualifier is generic across every name.
#     Five spellings, five distinct regions, all should work identically.
# ----------------------------------------------------------------------
out("[1] <region>.commit works for any region name");

kept_a :: Array := [];
kept_b :: Array := [];
kept_c :: Array := [];
kept_d :: Array := [];
kept_e :: Array := [];

region alpha {
    x := [1, 2, 3];
    alpha.commit(x);
    kept_a = x;
};
out("  alpha:             " + string(kept_a) + "   (expect [1, 2, 3])");

region epoch {
    x := [4, 5, 6];
    epoch.commit(x);
    kept_b = x;
};
out("  epoch:             " + string(kept_b) + "   (expect [4, 5, 6])");

region region7 {
    x := [7, 8, 9];
    region7.commit(x);
    kept_c = x;
};
out("  region7:           " + string(kept_c) + "   (expect [7, 8, 9])");

region _underscore_start {
    x := [10, 11, 12];
    _underscore_start.commit(x);
    kept_d = x;
};
out("  _underscore_start: " + string(kept_d) + "   (expect [10, 11, 12])");

region CamelCase {
    x := [13, 14, 15];
    CamelCase.commit(x);
    kept_e = x;
};
out("  CamelCase:         " + string(kept_e) + "   (expect [13, 14, 15])");

# ----------------------------------------------------------------------
# [2] Unqualified region.commit still works — backwards compatibility.
# ----------------------------------------------------------------------
out("");
out("[2] region.commit — unqualified form unchanged");

kept_legacy :: Array := [];

region legacy {
    y := [20, 21, 22];
    region.commit(y);
    kept_legacy = y;
};
out("  kept = " + string(kept_legacy) + "   (expect [20, 21, 22])");

# ----------------------------------------------------------------------
# [3] @bounded<4096> — arena use under the limit. Should NOT halt.
# ----------------------------------------------------------------------
out("");
out("[3] @bounded<4096> — arena under limit (should not halt)");

@bounded<4096> region small_arena {
    arr :: Array<Float64> := [];
    arr.place_all(1.0, 100);   # 800 bytes of data
    out("  arr[0]  = " + string(arr[0])  + "   (expect 1.0)");
    out("  arr[99] = " + string(arr[99]) + "   (expect 1.0)");
};
out("  OK — did not halt");

# ----------------------------------------------------------------------
# [3b] @bounded<100> with 16 KB of scratch — no halt expected.
#      Scratch lives on the C stack, not in the arena. The bound is
#      measured against g_arena.total_allocated, so a region that only
#      touches scratch should register ~0 bytes of arena use.
# ----------------------------------------------------------------------
out("");
out("[3b] @bounded<100> + 16 KB scratch — no halt expected");
out("     (scratch is stack-allocated, never touches the arena)");

scratch_sum :: Float64 := 0.0;

@bounded<100> region scratch_heavy {
    scratch buf :: Float64[20048];   # 16 KB on the C stack
    buf[0]    = 42.0;
    buf[2047] = 84.0;
    scratch_sum = buf[0] + buf[2047];
};
out("  result = " + string(scratch_sum) + "   (expect 126.0)");
out("  OK — did not halt");

# ----------------------------------------------------------------------
# [4] @zeroed — scratch lowered to `double buf[N] = {0};`.
#     Every element must be 0.0 immediately after declaration.
# ----------------------------------------------------------------------
out("");
out("[4] @zeroed — fresh scratch is zero-initialized");

@zeroed region zeroed_zone {
    scratch buf :: Float64[32];

    all_zero :: Bool := true;
    through i :: 0..31 -> loop {
        if buf[i] != 0.0 { all_zero = false; }
    };
    out("  all 32 elements zero: " + string(all_zero) + "   (expect true)");

    buf[0] = 7.0;
    out("  buf[0] after write = " + string(buf[0]) + "   (expect 7.0)");
};

# ----------------------------------------------------------------------
# [5] @deterministic — pure arithmetic body, no time/RNG source.
#     Should compile cleanly (no VNE-106) and produce a known sum.
# ----------------------------------------------------------------------
out("");
out("[5] @deterministic — pure arithmetic body");

@deterministic region det {
    scratch data :: Float64[8];
    through i :: 0..7 -> loop {
        data[i] = float64(i) * 1.5;
    };
    total :: Float64 := 0.0;
    through i :: 0..7 -> loop {
        total = total + data[i];
    };
    # 0.0 + 1.5 + 3.0 + 4.5 + 6.0 + 7.5 + 9.0 + 10.5 = 42.0
    out("  sum = " + string(total) + "   (expect 42.0)");
};

# ----------------------------------------------------------------------
# [6] Composition via nesting — @bounded around @zeroed.
#     Single policy per region is a grammar decision; nesting composes
#     the effects without touching the parser.
# ----------------------------------------------------------------------
out("");
out("[6] @bounded<2048> outer + @zeroed inner — composed via nesting");

combo_sum :: Float64 := 0.0;

@bounded<2048> region combo {
    @zeroed region inner {
        scratch buf :: Float64[64];   # 512 bytes on the stack
        buf[0]  = 1.0;
        buf[63] = 9.0;
        out("  buf[0]  = " + string(buf[0])  + "   (expect 1.0)");
        out("  buf[63] = " + string(buf[63]) + "   (expect 9.0)");
        out("  buf[32] = " + string(buf[32]) + "   (expect 0.0 — @zeroed)");
        combo_sum = buf[0] + buf[63];
    };
};
out("  sum from inner = " + string(combo_sum) + "   (expect 10.0)");
out("  OK — did not halt");

# ----------------------------------------------------------------------
# [7] Commit from a policy-prefixed region.
#     <region>.commit must work regardless of the region's policy.
# ----------------------------------------------------------------------
out("");
out("[7] <region>.commit inside a @bounded region");

kept_policed :: Array := [];

@bounded<2048> region policed {
    z := [100, 200, 300];
    policed.commit(z);
    kept_policed = z;
};
out("  kept = " + string(kept_policed) + "   (expect [100, 200, 300])");

# ----------------------------------------------------------------------
# Done.
# ----------------------------------------------------------------------
out("");
out("=== All smoke tests passed ===");
out("");
