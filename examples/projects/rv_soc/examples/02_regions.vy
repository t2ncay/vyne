# 02_regions.vy - the two region policies the demo relies on.
#
# Run with:
#   vynec run examples/02_regions.vy

ruleset { dynamic_casting };

out("=== regions ===");

@bounded<4096> region bounded_work {
    xs :: Array<Float64> := [];
    xs.place_all(1.0, 128);
    out("bounded sum = " + string(xs[127]));
};

@deterministic region deterministic_work {
    total :: Float64 := 0.0;
    through i :: 0..100 -> loop { total = total + float64(i); };
    out("deterministic sum = " + string(total));
};
