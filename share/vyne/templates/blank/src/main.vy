# main.vy - entry point for {{name}}

ruleset { dynamic_casting };

out("Hello from {{name}}!");

# @bounded regions have a runtime-verified memory ceiling.
# This region cannot exceed 1 KiB. The array below is sized so the
# region's allocation overhead still fits under the bound.
@bounded<1024> region demo {
    arr :: Array<Float64> := [];
    arr.place_all(1.0, 64);
    out("arr[63] = " + string(arr[63]));
};
out("✓ @bounded region completed without exceeding 1024 bytes");

# @deterministic regions are scan-checked for non-determinism sources.
@deterministic region det {
    total :: Float64 := 0.0;
    through i :: 0..10 -> loop { total = total + float64(i); };
    out("sum(0..10) = " + string(total));
};
out("✓ @deterministic region has no RNG or time sources");
