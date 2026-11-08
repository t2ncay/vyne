# region_slab.vy — @slab size-class region policy.
#
# Run compiled (the interpreter does not support region blocks):
#   ./vynec --compile examples/test/region_slab.vy
#   ./examples/test/region_slab
#
# Verifies:
#   - slab_alloc(n) returns an Array<Float64>/Array<Int64> of the requested size
#   - the backing store is zero-filled on first use and on reuse
#   - size-class reuse does not grow the arena after warm-up
#   - the region rewind reclaims every class's backing store

use native vmem;

out("=== @slab region test ===");

before :: Int64 := vmem.total_allocated();

@slab<Float64, 1024, 4> region f64_pool {
    a :: Array<Float64> := slab_alloc(100);   # class 128
    b :: Array<Float64> := slab_alloc(300);   # class 512

    out(a.size() == 100);
    out(b.size() == 300);
    out(a[0] == 0.0);
    out(b[0] == 0.0);

    # Dirty the slot, free it, then take the same-size class again.
    a[0] = 3.5;
    used_before :: Int64 := vmem.total_allocated();

    slab_free(a);
    c :: Array<Float64> := slab_alloc(100);   # reuses the just-freed slot

    used_after :: Int64 := vmem.total_allocated();
    out(used_before == used_after);          # no new backing store
    out(c[0] == 0.0);                        # reuse is zero-filled

    slab_free(b);
    slab_free(c);
};

out(vmem.total_allocated() == before);       # rewind reclaimed the slab

@slab<Int64, 256, 2> region i64_pool {
    x :: Array<Int64> := slab_alloc(10);      # class 16
    x[0] = 42;
    out(x.size() == 10);
    out(x[0] == 42);
    slab_free(x);
    out(x.size() == 0);
};

out("=== done ===");
