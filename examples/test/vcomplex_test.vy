# vcomplex_test.vy - scalar, polar, function, and split-array checks.

use external "vcomplex/vcomplex.vy";

failures :: Array<Int64> := [0];

fn close(a :: Float64, b :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d < 0.000001;
}

fn close_c(a :: vcomplex.Complex, b :: vcomplex.Complex) -> Bool {
    return vcomplex.equals(a, b, 0.000001);
}

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== vcomplex ===");

half_pi :: Float64 := 1.5707963267948966;
pi :: Float64 := 3.141592653589793;

i :: vcomplex.Complex := vcomplex.Complex(0.0, 1.0);
check("i-squared", close_c(vcomplex.mul(i, i), vcomplex.Complex(-1.0, 0.0)));

z :: vcomplex.Complex := vcomplex.Complex(1.0, 1.0);
check("z-squared", close_c(vcomplex.mul(z, z), vcomplex.Complex(0.0, 2.0)));

check("abs-3-4", close(vcomplex.abs(vcomplex.Complex(3.0, 4.0)), 5.0));
check("abs-sq", close(vcomplex.abs_sq(vcomplex.Complex(3.0, 4.0)), 25.0));
check("arg-i", close(vcomplex.arg(i), half_pi));
check("conj", close_c(vcomplex.conj(z), vcomplex.Complex(1.0, -1.0)));
check("neg", close_c(vcomplex.neg(z), vcomplex.Complex(-1.0, -1.0)));
check("scale", close_c(vcomplex.scale(z, 2.0), vcomplex.Complex(2.0, 2.0)));

check("div", close_c(
    vcomplex.div(vcomplex.Complex(1.0, 2.0), vcomplex.Complex(1.0, -1.0)),
    vcomplex.Complex(-0.5, 1.5)
));

check("exp-i-pi-2", close_c(
    vcomplex.exp(vcomplex.Complex(0.0, half_pi)),
    vcomplex.Complex(0.0, 1.0)
));
check("log-e", close_c(
    vcomplex.log(vcomplex.exp(vcomplex.Complex(1.0, 0.0))),
    vcomplex.Complex(1.0, 0.0)
));
check("sqrt-minus-1", close_c(
    vcomplex.sqrt(vcomplex.Complex(-1.0, 0.0)),
    vcomplex.Complex(0.0, 1.0)
));
check("pow", close_c(
    vcomplex.pow(vcomplex.Complex(2.0, 0.0), vcomplex.Complex(3.0, 0.0)),
    vcomplex.Complex(8.0, 0.0)
));
check("sin-zero", close_c(vcomplex.sin(vcomplex.Complex(0.0, 0.0)),
                          vcomplex.Complex(0.0, 0.0)));
check("cos-zero", close_c(vcomplex.cos(vcomplex.Complex(0.0, 0.0)),
                          vcomplex.Complex(1.0, 0.0)));
check("tan-zero", close_c(vcomplex.tan(vcomplex.Complex(0.0, 0.0)),
                          vcomplex.Complex(0.0, 0.0)));

check("from-polar", close_c(
    vcomplex.from_polar(2.0, half_pi),
    vcomplex.Complex(0.0, 2.0)
));
p :: vcomplex.Polar := vcomplex.to_polar(vcomplex.Complex(0.0, 2.0));
check("to-polar-r", close(p.r, 2.0));
check("to-polar-theta", close(p.theta, half_pi));
check("rotate", close_c(
    vcomplex.rotate(vcomplex.Complex(1.0, 0.0), half_pi),
    vcomplex.Complex(0.0, 1.0)
));
check("scale-polar", close(vcomplex.scale_polar(p, 3.0).r, 6.0));

# ---------------------------------------------------------------------------
# Split arrays
# ---------------------------------------------------------------------------

re_a :: Array<Float64> := [1.0, 2.0];
im_a :: Array<Float64> := [3.0, 4.0];
re_b :: Array<Float64> := [5.0, 6.0];
im_b :: Array<Float64> := [7.0, 8.0];
a :: vcomplex.ComplexArray := vcomplex.from_parts(re_a, im_a, 2);
b :: vcomplex.ComplexArray := vcomplex.from_parts(re_b, im_b, 2);

sum_ab :: vcomplex.ComplexArray := vcomplex.a_add(a, b);
check("a_add-re", close(sum_ab.re[0], 6.0));
check("a_add-im", close(sum_ab.im[0], 10.0));

dif_ab :: vcomplex.ComplexArray := vcomplex.a_sub(b, a);
check("a_sub-re", close(dif_ab.re[0], 4.0));
check("a_sub-im", close(dif_ab.im[0], 4.0));

mul_ab :: vcomplex.ComplexArray := vcomplex.a_mul(a, b);
check("a_mul-re0", close(mul_ab.re[0], -16.0));
check("a_mul-im0", close(mul_ab.im[0], 22.0));
check("a_mul-re1", close(mul_ab.re[1], -20.0));
check("a_mul-im1", close(mul_ab.im[1], 40.0));

scaled :: vcomplex.ComplexArray := vcomplex.a_scale(a, 2.0);
check("a_scale-re", close(scaled.re[0], 2.0));
check("a_scale-im", close(scaled.im[0], 6.0));

conj_a :: vcomplex.ComplexArray := vcomplex.a_conj(a);
check("a_conj-im", close(conj_a.im[0], -3.0));

abs_a :: Array<Float64> := vcomplex.a_abs(a);
check("a_abs-0", close(abs_a[0], 3.1622776601683795));
abs_sq_a :: Array<Float64> := vcomplex.a_abs_sq(a);
check("a_abs_sq-0", close(abs_sq_a[0], 10.0));
arg_a :: Array<Float64> := vcomplex.a_arg(a);
check("a_arg-0", close(arg_a[0], 1.2490457723982544));

sum_a :: vcomplex.Complex := vcomplex.a_sum(a);
check("a_sum-re", close(sum_a.re, 3.0));
check("a_sum-im", close(sum_a.im, 7.0));

dot_aa :: vcomplex.Complex := vcomplex.a_dot(a, a);
check("a_dot-re", close(dot_aa.re, -20.0));
check("a_dot-im", close(dot_aa.im, 22.0));

rs :: Array<Float64> := [2.0, 1.0];
thetas :: Array<Float64> := [half_pi, 0.0];
polar_a :: vcomplex.ComplexArray := vcomplex.a_from_polar(rs, thetas, 2);
check("a_from_polar-re0", close(polar_a.re[0], 0.0));
check("a_from_polar-im0", close(polar_a.im[0], 2.0));
check("a_from_polar-re1", close(polar_a.re[1], 1.0));
check("a_from_polar-im1", close(polar_a.im[1], 0.0));

if failures[0] > 0 {
    out("vcomplex: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vcomplex: all checks passed");
