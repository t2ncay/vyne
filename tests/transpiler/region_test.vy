# =====================================================================
# region_test.vy — end-to-end test for the vmem region ecosystem.
#
# Exercises the four things the recent fixes cover:
#
#   1. Baseline: no region → arena grows.
#   2. Region flat: rewind keeps total_allocated constant across iters.
#   3. break / continue out of a region must fire the rewind.
#   4. Nested regions: inner rewinds per iter, outer rewinds once.
#   5. region.commit: a String survives the rewind (commit-arena path).
#   6. return primitive out of a region (rewind path).
#   7. return non-primitive out of a region (pop-checkpoints path).
#   8. return String out of a region (pop-checkpoints path).
#
# Run:
#     vyne build region_test.vy && ./region_test
#
# Expected output is summarised at the bottom of this file.
# =====================================================================

ruleset { dynamic_casting };

module vmem;

# ---------------------------------------------------------------------
# 1. Baseline — no region, arena grows.
# ---------------------------------------------------------------------
out("-- 1 baseline --");

b0 :: Int64 = vmem.total_allocated();

through i :: 1..20 -> loop {
    xs :: Array<Int64> = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
};

b1 :: Int64 = vmem.total_allocated();

if (b1 > b0) {
    out("PASS: baseline grew");
} else {
    out("FAIL: baseline did not grow");
}


# ---------------------------------------------------------------------
# 2. Region flat — arena returns to its starting offset every iteration.
# ---------------------------------------------------------------------
out("-- 2 region flat --");

r0 :: Int64 = vmem.total_allocated();

through i :: 1..20 -> loop {
    region scratch {
        xs :: Array<Int64> = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
        ys :: Array<Float64> = [1.0, 2.0, 3.0, 4.0, 5.0];
    };
};

r1 :: Int64 = vmem.total_allocated();

if (r1 == r0) {
    out("PASS: region is flat");
} else {
    out("FAIL: region leaked " + string(r1 - r0) + " bytes");
}


# ---------------------------------------------------------------------
# 3. break out of a region — rewind must fire on the way out.
# ---------------------------------------------------------------------
out("-- 3 break --");

br0 :: Int64 = vmem.total_allocated();

through i :: 1..100 -> loop {
    region scratch {
        xs :: Array<Int64> = [1, 2, 3, 4, 5, 6, 7, 8];
        if (i == 5) {
            break;
        }
    };
};

br1 :: Int64 = vmem.total_allocated();

if (br1 == br0) {
    out("PASS: break rewound");
} else {
    out("FAIL: break leaked " + string(br1 - br0) + " bytes");
}


# ---------------------------------------------------------------------
# 4. continue out of a region — rewind must fire on the way out.
# ---------------------------------------------------------------------
out("-- 4 continue --");

cn0 :: Int64 = vmem.total_allocated();

through i :: 1..20 -> loop {
    region scratch {
        xs :: Array<Int64> = [1, 2, 3, 4, 5, 6, 7, 8];
        if (i == 10) {
            continue;
        }
    };
};

cn1 :: Int64 = vmem.total_allocated();

if (cn1 == cn0) {
    out("PASS: continue rewound");
} else {
    out("FAIL: continue leaked " + string(cn1 - cn0) + " bytes");
}


# ---------------------------------------------------------------------
# 5. Nested regions — inner rewinds per iteration, outer once.
# ---------------------------------------------------------------------
out("-- 5 nested --");

n0 :: Int64 = vmem.total_allocated();

through i :: 1..10 -> loop {
    region outer {
        a :: Array<Int64> = [1, 2, 3, 4];
        through j :: 1..5 -> loop {
            region inner {
                b :: Array<Int64> = [5, 6, 7, 8];
            };
        };
    };
};

n1 :: Int64 = vmem.total_allocated();

if (n1 == n0) {
    out("PASS: nested rewound");
} else {
    out("FAIL: nested leaked " + string(n1 - n0) + " bytes");
}


# ---------------------------------------------------------------------
# 6. region.commit — a String survives the rewind.
#
#     The String is produced by concatenation (arena-backed, not a
#     rodata literal), assigned to the outer variable, then committed.
#     After the region closes, the outer variable must still read the
#     committed value.
# ---------------------------------------------------------------------
out("-- 6 commit --");

last_msg :: String = "initial";

through epoch :: 1..4 -> loop {
    region scratch {
        xs :: Array<Int64> = [epoch, epoch + 10, epoch + 20];
        msg :: String = "epoch:" + string(epoch);
        last_msg = msg;
        region.commit(last_msg);
    };
    out(last_msg);
};


# ---------------------------------------------------------------------
# 7. Return a primitive out of a region.
#
#     The static type of `42` is Int64, so the emitter captures it in
#     a local, rewinds the arena, and returns the local.
# ---------------------------------------------------------------------
fn answer() -> Int64 {
    region scratch {
        xs :: Array<Int64> = [100, 200, 300];
        return 42;
    };
    return 0;
}

out("-- 7 return primitive --");
out(answer());


# ---------------------------------------------------------------------
# 8. Return a non-primitive out of a region.
#
#     Static type is Array, so the emitter pops the checkpoint without
#     rewinding; the array stays live for the caller.
# ---------------------------------------------------------------------
fn get_arr() -> Array {
    region scratch {
        xs :: Array<Int64> = [7, 8, 9];
        return xs;
    };
    return [0];
}

out("-- 8 return array --");
out(get_arr());


# ---------------------------------------------------------------------
# 9. Return a String out of a region (non-primitive, arena-backed).
# ---------------------------------------------------------------------
fn get_msg() -> String {
    region scratch {
        msg :: String = "hello-" + "world";
        return msg;
    };
    return "empty";
}

out("-- 9 return string --");
out(get_msg());