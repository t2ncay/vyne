# region_move.vy — zero-copy region → commit transfer.
#
# Run compiled (regions and region.move are compiled-backend features):
#   ./vynec --compile examples/test/region_move.vy
#   ./examples/test/region_move
#
# Verifies:
#   - region.move(x) as an expression returns the moved value, so an
#     outer variable declared before the region can take it without an
#     escape error
#   - a large (> arena block) payload is spliced out of the region arena
#     instead of deep-copied: region live bytes fall by the payload size
#     and commit bytes rise by the same amount
#   - the moved value survives the region rewind with its contents
#   - the statement form works and falls back to a copy for small arrays

use native vmem;
use external "vlin/vlin.vy";

out("=== region.move test ===");

commit_baseline :: Int64 := vmem.commit_total_allocated();

persistent_model :: Array<Float64> := [];
main_baseline :: Int64 := vmem.total_allocated();

# 2_000_000 Float64 = 16 MiB final payload (2^21 elements). That is
# larger than VYNE_ARENA_BLOCK_SIZE, so it owns a dedicated block and
# the move can change block ownership in O(1).
@bounded<64_000_000> region train {
    local_model :: Array<Float64> := vlin.zeros_f64(2_000_000);
    local_model[0] = 42.5;
    local_model[1] = -1.0;
    out(local_model.size() == 2_000_000);

    live_before :: Int64 := vmem.total_allocated();
    persistent_model = region.move(local_model);
    live_after :: Int64 := vmem.total_allocated();

    # A zero-copy move removes the 16 MiB payload from the region arena.
    out(live_before - live_after >= 16_000_000);
    out(persistent_model.size() == 2_000_000);
    out(persistent_model[0] == 42.5);

    # The source is rebound to the moved storage.
    out(local_model.size() == 2_000_000);
    out(local_model[0] == 42.5);
};

# The region rewind must not free the moved payload.
out(persistent_model.size() == 2_000_000);
out(persistent_model[0] == 42.5);
out(persistent_model[1] == -1.0);
out(vmem.commit_total_allocated() - commit_baseline >= 16_000_000);
out(vmem.total_allocated() == main_baseline);

# --- statement form on a small array: falls back to a commit copy ----
small :: Array<Float64> := [];
small_baseline :: Int64 := vmem.total_allocated();
region move_small {
    local :: Array<Float64> := vlin.zeros_f64(64);
    local[0] = 9.25;
    region.move(local);
    out(local.size() == 64);
    out(local[0] == 9.25);
    small = local;
};

out(small.size() == 64);
out(small[0] == 9.25);
out(vmem.total_allocated() == small_baseline);
out("=== done ===");
