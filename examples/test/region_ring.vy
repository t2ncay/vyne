# region_ring.vy — @ring rotating-slot region policy.
#
# Run compiled (regions are a compiled-backend feature):
#   ./vynec --compile examples/test/region_ring.vy
#   ./examples/test/region_ring
#
# Verifies:
#   - ring_alloc(n) returns an Array of the requested size, capped by the slot
#   - the slot is zero-filled on first use and on reuse
#   - after N allocations the oldest slot is overwritten (bounded history)
#   - the ring never grows after region entry
#   - the region rewind reclaims the ring backing store

use native vmem;

out("=== @ring region test ===");

before :: Int64 := vmem.total_allocated();

@ring<Float64, 4, 16> region stream {
    a :: Array<Float64> := ring_alloc(16);
    b :: Array<Float64> := ring_alloc(16);
    c :: Array<Float64> := ring_alloc(16);
    d :: Array<Float64> := ring_alloc(16);

    out(a.size() == 16);
    out(b.size() == 16);
    out(c.size() == 16);
    out(d.size() == 16);

    a[0] = 1.0;
    b[0] = 2.0;
    c[0] = 3.0;
    d[0] = 4.0;
    out(a[0] == 1.0);
    out(d[0] == 4.0);

    used_before :: Int64 := vmem.total_allocated();
    e :: Array<Float64> := ring_alloc(8);   # overwrites slot 0 (a)
    used_after :: Int64 := vmem.total_allocated();

    out(used_before == used_after);          # ring does not grow
    out(e.size() == 8);
    out(e[0] == 0.0);                        # zero-filled on reuse

    e[0] = 5.0;
    out(a[0] == 5.0);                        # a aliases the rotated slot
};

out(vmem.total_allocated() == before);       # rewind reclaimed the ring

@ring<Int64, 2, 4> region istream {
    x :: Array<Int64> := ring_alloc(4);
    x[0] = 42;
    out(x[0] == 42);

    y :: Array<Int64> := ring_alloc(4);
    out(y[0] == 0);

    z :: Array<Int64> := ring_alloc(4);      # wraps to slot 0
    out(z[0] == 0);                          # zero-filled reuse
    z[0] = 99;
    out(x[0] == 99);                         # alias shows the wrap
};

out("=== done ===");
