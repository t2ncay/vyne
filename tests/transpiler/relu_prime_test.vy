# tests/scratch/relu_prime_test.vy
ruleset { dynamic_casting };

module vmath;

# form A: the workaround (loop + push)
fn form_a(v :: Array<Float64>) -> Array {
    output :: Array = [];
    through i :: 0..v.size()-1 -> loop {
        x :: Float64 = v[i];
        if x > 0.0 {
            output.push(1.0);
        } else {
            output.push(0.0);
        }
    };
    return output;
}

# form B: the collect form from circle.md
fn form_b(v :: Array<Float64>) -> Array {
    return through x :: v -> collect {
        if x > 0.0 { 1.0 } else { 0.0 }
    };
}

out(form_a([-1.0, 0.0, 1.0, 2.0]));
out(form_b([-1.0, 0.0, 1.0, 2.0]));