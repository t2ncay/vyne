# SAFETY: correct scratch indexing.
# Expected: PASS.

ruleset { dynamic_casting };

region r {
    scratch buf :: Float64[8];
    through i :: 0..7 -> loop { buf[i] = float64(i); };
    total :: Float64 := 0.0;
    through i :: 0..7 -> loop { total = total + buf[i]; };
    out("correct_index: total=" + string(total));
};
