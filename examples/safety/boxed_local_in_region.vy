# SAFETY: a boxed local declared inside a region and re-assigned
# inside the same region must not trigger VNE-070.
# Expected: PASS.

ruleset { dynamic_casting };

region r {
    # Different branch types keep the local boxed rather than inferring Float64.
    choose_float :: Bool := true;
    acc := choose_float ? 0.0 : null;
    through i :: 0..9 -> loop {
        acc = acc + float64(i);
    };
    out("boxed_local_in_region: acc=" + string(acc));
};
