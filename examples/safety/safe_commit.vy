# SAFETY: escape via region.commit is allowed.
# Expected: PASS.

fn test() {
    outer :: Array := [];
    region r {
        # Keep the value boxed, matching the outer Array representation.
        choose_values :: Bool := true;
        inner :: Array := choose_values ? [1.0, 2.0, 3.0] : [];
        r.commit(inner);
        outer = inner;
    };
    region reuse {
        replacement :: Array := [9.0, 8.0, 7.0];
        if replacement[0] != 9.0 { exit(1); }
    };
    if outer.size() != 3 || outer[0] != 1.0 || outer[2] != 3.0 { exit(1); }
    out("safe_commit: len=" + string(outer.size()));
}

test();
