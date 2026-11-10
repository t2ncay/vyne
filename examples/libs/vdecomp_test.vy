# vdecomp_test.vy - LU, QR, Cholesky, eigen, and singular values.
#
#   ./vynec --compile examples/libs/vdecomp_test.vy
#   ./examples/libs/vdecomp_test

use external "vdecomp/vdecomp.vy";
use native vmath;

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

fn close(a :: Float64, b :: Float64, eps :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d <= eps;
}

out("=== vdecomp ===");

# ---- LU -------------------------------------------------------------
a2 :: Array<Float64> := [4.0, 1.0, 2.0, 3.0];
A2 :: vdecomp.Matrix := vdecomp.Matrix(a2, 2, 2);
lu2 :: vdecomp.LUResult := vdecomp.lu(a2, 2);
check("lu-det", close(lu2.det, 10.0, 1e-12));
check("lu-not-singular", lu2.singular == false);

b2 :: Array<Float64> := [1.0, 2.0];
x2 :: Array<Float64> := vdecomp.solve_lu(lu2, b2, 2);
# A^-1 b = [0.1, 0.6]
check("lu-solve-0", close(x2[0], 0.1, 1e-12));
check("lu-solve-1", close(x2[1], 0.6, 1e-12));

check("det-wrapper", close(vdecomp.determinant(A2), 10.0, 1e-12));

inv :: vdecomp.Matrix := vdecomp.inverse(A2);
# A^-1 = [[0.3, -0.1], [-0.2, 0.4]]
check("inverse-00", close(inv.data[0], 0.3, 1e-12));
check("inverse-01", close(inv.data[1], -0.1, 1e-12));
check("inverse-10", close(inv.data[2], -0.2, 1e-12));
check("inverse-11", close(inv.data[3], 0.4, 1e-12));

sol :: vdecomp.Series := vdecomp.solve(A2, vdecomp.Series(b2, 2));
check("solve-0", close(sol.data[0], 0.1, 1e-12));
check("solve-1", close(sol.data[1], 0.6, 1e-12));

# ---- QR / least squares --------------------------------------------
qra :: Array<Float64> := [1.0, 0.0, 0.0, 1.0, 1.0, 1.0];
qrm :: vdecomp.Matrix := vdecomp.Matrix(qra, 3, 2);
qrb :: vdecomp.Series := vdecomp.Series([1.0, 2.0, 3.0], 3);
ls :: vdecomp.Series := vdecomp.least_squares(qrm, qrb);
check("least-squares-0", close(ls.data[0], 1.0, 1e-12));
check("least-squares-1", close(ls.data[1], 2.0, 1e-12));

qrres :: vdecomp.QRResult := vdecomp.qr_matrix(qrm);
# Q^T Q = I for the 3x3 Q.
ortho_ok :: Bool := true;
through i :: 0..2 -> loop {
    through j :: 0..2 -> loop {
        s :: Float64 := 0.0;
        through k :: 0..2 -> loop { s = s + qrres.q[k * 3 + i] * qrres.q[k * 3 + j]; };
        target :: Float64 := 0.0;
        if i == j { target = 1.0; }
        if !close(s, target, 1e-10) { ortho_ok = false; }
    };
};
check("qr-orthogonal", ortho_ok);

# ---- Cholesky -------------------------------------------------------
s2 :: Array<Float64> := [4.0, 1.0, 1.0, 3.0];
S2 :: vdecomp.Matrix := vdecomp.Matrix(s2, 2, 2);
chol :: vdecomp.CholeskyResult := vdecomp.cholesky(s2, 2);
check("cholesky-ok", chol.ok);
check("cholesky-l00", close(chol.l[0], 2.0, 1e-12));
check("cholesky-l11", close(chol.l[3], vmath.sqrt(2.75), 1e-12));

spd_x :: vdecomp.Series := vdecomp.solve_spd(S2, vdecomp.Series([1.0, 2.0], 2));
check("spd-solve-0", close(spd_x.data[0], 1.0 / 11.0, 1e-12));
check("spd-solve-1", close(spd_x.data[1], 7.0 / 11.0, 1e-12));

# ---- Symmetric eigen ------------------------------------------------
e2 :: Array<Float64> := [2.0, 1.0, 1.0, 2.0];
E2 :: vdecomp.Matrix := vdecomp.Matrix(e2, 2, 2);
eig2 :: vdecomp.EigenResult := vdecomp.eigen_matrix(E2);
lo :: Float64 := eig2.values[0];
hi :: Float64 := eig2.values[1];
if lo > hi { lo = eig2.values[1]; hi = eig2.values[0]; }
check("eigen-2x2-low", close(lo, 1.0, 1e-10));
check("eigen-2x2-high", close(hi, 3.0, 1e-10));

e3 :: Array<Float64> := [2.0, 1.0, 0.0, 1.0, 2.0, 0.0, 0.0, 0.0, 3.0];
eig3 :: vdecomp.EigenResult := vdecomp.eigen_symmetric(e3, 3);
sum3 :: Float64 := eig3.values[0] + eig3.values[1] + eig3.values[2];
prod3 :: Float64 := eig3.values[0] * eig3.values[1] * eig3.values[2];
check("eigen-3x3-trace", close(sum3, 7.0, 1e-10));
check("eigen-3x3-det", close(prod3, 9.0, 1e-10));

# ---- Singular values ------------------------------------------------
svm :: Array<Float64> := [3.0, 0.0, 0.0, 2.0];
SVM :: vdecomp.Matrix := vdecomp.Matrix(svm, 2, 2);
sv :: Array<Float64> := vdecomp.singular_values(SVM);
check("singular-0", close(sv[0], 3.0, 1e-10));
check("singular-1", close(sv[1], 2.0, 1e-10));

if failures[0] != 0 {
    out(string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
