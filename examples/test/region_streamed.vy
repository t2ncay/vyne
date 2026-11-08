# region_streamed.vy — @streamed pinned-memory region policy.
#
# Run compiled (regions are a compiled-backend feature):
#   ./vynec --compile examples/test/region_streamed.vy
#   ./examples/test/region_streamed
#
# Verifies:
#   - streamed_alloc(n) returns an Array of the requested size, zero-filled
#   - the stream arena reports live bytes and a block count
#   - the platform reports whether the memory was actually pinned (0/1)
#   - nested @streamed frames reclaim independently
#   - the outer region's leave reclaims the pinned blocks and the main arena

use native vmem;

out("=== @streamed region test ===");

# Early return must unwind the stream frame as well as the main checkpoint.
fn early_exit(flag :: Bool) -> Int64 {
    @streamed<Float64, 32> region inner {
        a :: Array<Float64> := streamed_alloc(8);
        a[0] = 1.0;
        if flag {
            return 7;
        }
    };
    return 8;
}

before :: Int64 := vmem.stream_total_allocated();

@streamed<Float64, 64> region packet {
    a :: Array<Float64> := streamed_alloc(16);
    out(a.size() == 16);
    out(a[0] == 0.0);

    used1 :: Int64 := vmem.stream_total_allocated();
    out(used1 >= 128);                       # 16 * 8 bytes
    out(vmem.stream_block_count() >= 1);

    pinned :: Int64 := vmem.stream_is_pinned();
    out(pinned == 0 || pinned == 1);
    out(pinned);                             # raw provider status

    b :: Array<Float64> := streamed_alloc(32);
    b[0] = 7.0;
    out(b[0] == 7.0);

    used2 :: Int64 := vmem.stream_total_allocated();
    out(used2 > used1);

    @streamed<Int64, 16> region nested {
        x :: Array<Int64> := streamed_alloc(4);
        x[0] = 42;
        out(x[0] == 42);
        out(vmem.stream_total_allocated() > used2);
    };

    out(vmem.stream_total_allocated() == used2);
};

out(vmem.stream_total_allocated() == before);
out(vmem.stream_block_count() == 0);

out(early_exit(true) == 7);                  # early return inside @streamed
out(vmem.stream_total_allocated() == 0);
out(early_exit(false) == 8);                 # normal close
out(vmem.stream_total_allocated() == 0);

out("=== done ===");
