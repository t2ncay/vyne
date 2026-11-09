# region_drop.vy — LIFO early-drop reclamation.
#
# Run compiled (regions and drop are compiled-backend features):
#   ./vynec --compile examples/test/region_drop.vy
#   ./examples/test/region_drop
#
# Verifies:
#   - drop(x) reclaims a typed array payload when it is the arena tail
#   - a successful drop shrinks total_allocated and empties the container
#   - the freed space is reused by the next allocation, so two large
#     buffers can fit in a @bounded region that only has room for one
#   - drop(x) on a non-tail object is a no-op and leaves it usable
#   - the region rewind still releases everything at close

use native vmem;
use external "vlin/vlin.vy";

out("=== drop / early lifetime test ===");

baseline :: Int64 := vmem.total_allocated();

# --- [1] tail drop reclaims and the space is reused -----------------
@bounded<12_000> region frame {
    big :: Array<Float64> := vlin.zeros_f64(1024);   # 8 KiB payload
    out(big.size() == 1024);
    big[0] = 3.5;
    out(big[0] == 3.5);

    live_before :: Int64 := vmem.total_allocated();
    drop(big);
    live_after :: Int64 := vmem.total_allocated();

    out(live_after < live_before);
    out(big.size() == 0);

    # Without the drop above this second 8 KiB payload would push the
    # region past @bounded<12_000>. It fits because the first was freed.
    huge :: Array<Float64> := vlin.zeros_f64(1024);
    out(huge.size() == 1024);
    huge[0] = 7.0;
    out(huge[0] == 7.0);
};

out(vmem.total_allocated() == baseline);

# --- [2] non-LIFO drop is a no-op -----------------------------------
region non_lifo {
    first :: Array<Float64> := vlin.zeros_f64(64);
    second :: Array<Float64> := vlin.zeros_f64(64);
    first[0] = 1.0;
    second[0] = 2.0;

    live_before :: Int64 := vmem.total_allocated();
    drop(first);                  # first is not the most recent allocation
    live_after :: Int64 := vmem.total_allocated();

    out(live_after == live_before);
    out(first.size() == 64);
    out(first[0] == 1.0);
    out(second.size() == 64);
    out(second[0] == 2.0);
};

out(vmem.total_allocated() == baseline);

# --- [3] dropping twice is harmless --------------------------------
region double_drop {
    buf :: Array<Float64> := vlin.zeros_f64(128);
    drop(buf);
    out(buf.size() == 0);
    drop(buf);                    # already empty: still a no-op
    out(buf.size() == 0);
};

out(vmem.total_allocated() == baseline);
out("=== done ===");
