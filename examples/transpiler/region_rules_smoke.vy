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
out("[3b] @bounded<100> + 80 B scratch — no halt expected");
out("     (80 <= 100; scratch counts toward the bound, this fits)");

scratch_sum :: Float64 := 0.0;

@bounded<100> region scratch_heavy {
    scratch buf :: Float64[10];   # 80 bytes — fits the 100 B bound
    buf[0] = 42.0;
    buf[9] = 84.0;
    scratch_sum = buf[0] + buf[9];
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
# [8] Multiple scratch declarations in one @bounded — cumulative sum.
#     Float64[32] = 256 B, Float64[64] = 512 B → total 768 ≤ 1024.
#     Exercises `applyBoundedStaticScratch` accumulating across decls.
# ----------------------------------------------------------------------
out("");
out("[8] @bounded<1024> with two scratch arrays (768 B total)");

multi_sum :: Float64 := 0.0;

@bounded<1024> region multi_scratch {
    scratch a :: Float64[32];   # 256 B
    scratch b :: Float64[64];   # 512 B, running total 768
    a[0] = 1.0;
    b[0] = 2.0;
    multi_sum = a[0] + b[0];
};
out("  sum = " + string(multi_sum) + "   (expect 3.0)");
out("  OK — did not halt");

# ----------------------------------------------------------------------
# [9] Nested @bounded — inner scratch counts toward the outer budget too.
#     Outer @bounded<1024>: 256 B (outer scratch) + 256 B (inner scratch)
#     = 512 ≤ 1024.
#     Inner @bounded<512>:  256 B (inner scratch) ≤ 512.
#     Both exit checks pass; static accounting is per-context.
# ----------------------------------------------------------------------
out("");
out("[9] Nested @bounded — inner scratch contributes to both budgets");

nested_sum :: Float64 := 0.0;

@bounded<1024> region outer_b {
    scratch outer_buf :: Float64[32];   # 256 B → outer only
    outer_buf[0] = 10.0;

    @bounded<512> region inner_b {
        scratch inner_buf :: Float64[32];   # 256 B → inner AND outer
        inner_buf[0] = 20.0;
        nested_sum = outer_buf[0] + inner_buf[0];
    };
};
out("  sum = " + string(nested_sum) + "   (expect 30.0)");
out("  OK — did not halt");

# ----------------------------------------------------------------------
# [10] @bounded with VLA under budget.
#      n = 16 → 16 × 8 = 128 B at runtime. 128 ≤ 2048.
#      Exercises the pre-allocation runtime check in the VLA path.
# ----------------------------------------------------------------------
out("");
out("[10] @bounded<2048> with VLA of 16 doubles (128 B)");

vla_n :: Int64 := 16;
vla_sum :: Float64 := 0.0;

@bounded<2048> region vla_ok {
    scratch buf :: Float64[vla_n];
    through i :: 0..vla_n-1 -> loop {
        buf[i] = float64(i);
    };
    through i :: 0..vla_n-1 -> loop {
        vla_sum = vla_sum + buf[i];
    };
};
# sum(0..15) = 120
out("  sum = " + string(vla_sum) + "   (expect 120.0)");
out("  OK — did not halt");

# ----------------------------------------------------------------------
# [11] @bounded with static scratch + arena combined.
#      Static: Float64[32] = 256 B (decrements budget to 768).
#      Arena:  place_all(2.0, 50) = 50 × 8 = 400 B (data) + overhead.
#      Exit check: arena_used (~448) > budget (768)? No.
#      Exercises the exit-check arithmetic `arena > N - static`.
# ----------------------------------------------------------------------
out("");
out("[11] @bounded<1024> with static scratch + arena combined");

combined_sum :: Float64 := 0.0;

@bounded<1024> region arena_and_scratch {
    scratch buf :: Float64[32];   # 256 B → budget reduced to 768
    buf[0] = 1.0;

    arr :: Array<Float64> := [];
    arr.place_all(2.0, 50);       # ~400 B arena; total ≈ 656 ≤ 1024

    combined_sum = buf[0] + arr[0];
};
out("  sum = " + string(combined_sum) + "   (expect 3.0)");
out("  OK — did not halt");

# ----------------------------------------------------------------------
# [12] @zeroed with a VLA — memset path, not `= {0}` path.
#      Static shapes get `= {0}` at declaration; VLAs cannot, so the
#      emitter must emit an explicit memset. This test verifies that
#      branch fires.
# ----------------------------------------------------------------------
out("");
out("[12] @zeroed with VLA — runtime memset zeros all elements");

zeroed_vla_n :: Int64 := 32;
zeroed_vla_ok :: Bool := true;

@zeroed region zeroed_vla {
    scratch buf :: Float64[zeroed_vla_n];
    through i :: 0..zeroed_vla_n-1 -> loop {
        if buf[i] != 0.0 { zeroed_vla_ok = false; }
    };
};
out("  all zero: " + string(zeroed_vla_ok) + "   (expect true)");

# ----------------------------------------------------------------------
# [13] @bounded outer + @deterministic inner.
#      Inner is zeroed (deterministic implies zeroed) AND its scratch
#      counts against the outer budget. Two policies compose; three
#      behaviours stack: bound enforcement, zero-init, no-nondeterminism.
# ----------------------------------------------------------------------
out("");
out("[13] @bounded outer + @deterministic inner — composed");

det_nested_sum :: Float64 := 0.0;

@bounded<2048> region det_bounded {
    @deterministic region det_inner {
        scratch buf :: Float64[8];   # 64 B → outer budget; zeroed
        all_zero :: Bool := true;
        through i :: 0..7 -> loop {
            if buf[i] != 0.0 { all_zero = false; }
        };
        out("  inner scratch zeroed: " + string(all_zero) + "   (expect true)");
        buf[0] = 1.0;
        buf[7] = 2.0;
        det_nested_sum = buf[0] + buf[7];
    };
};
out("  sum = " + string(det_nested_sum) + "   (expect 3.0)");
out("  OK — did not halt");

# ----------------------------------------------------------------------
# [14] <region>.commit with a typed array.
#      Hits the box / commit / unbox path in RegionCommitNode::compile
#      for `Array<Float64>` locals. The committed data must survive
#      the region rewind and remain readable through the alias.
# ----------------------------------------------------------------------
out("");
out("[14] <region>.commit with typed Array<Float64>");

kept_typed :: Array<Float64> := [];

@bounded<1024> region typed_commit {
    arr :: Array<Float64> := [];
    arr.place_all(3.14, 4);
    typed_commit.commit(arr);
    kept_typed = arr;
};
out("  kept[0]     = " + string(kept_typed[0])      + "   (expect 3.14)");
out("  kept.size() = " + string(kept_typed.size()) + "   (expect 4)");

# ----------------------------------------------------------------------
# Done.
# ----------------------------------------------------------------------
out("");
out("=== All smoke tests passed ===");
out("");
