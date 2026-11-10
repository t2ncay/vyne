# 03_cuda.vy - a first @device region.
#
# This project was generated with CUDA enabled. If the driver or
# device is unavailable at run time, Vyne falls back to the CPU backend.
# 
#
#   vynec run examples/03_cuda.vy

ruleset { dynamic_casting };

out("=== @device example ===");

@device<cuda> region gpu {
    out("backend:   " + device_backend());
    out("available: " + string(device_available()));

    n :: Int64 := 8;
    a :: Array<Float32> := [];
    b :: Array<Float32> := [];
    c :: Array<Float32> := [];
    through i :: 0..n-1 -> loop {
        a.push(float32(i));
        b.push(float32(2 * i));
        c.push(0.0f);
    };

    da :: Int64 := device_alloc(n);
    db :: Int64 := device_alloc(n);
    dc :: Int64 := device_alloc(n);
    device_upload(da, a, n);
    device_upload(db, b, n);
    device_add(dc, da, db, n);
    device_download(dc, c, n);

    out("c[7] = " + string(c[7]));
};
