# vsparse_test.vy - CSR construction, SpMV, transpose, and CG.
#
#   ./vynec --compile examples/libs/vsparse_test.vy
#   ./examples/libs/vsparse_test

use external "vsparse/vsparse.vy";
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

out("=== vsparse ===");

# Non-symmetric matrix for transpose/SpMV checks:
#   [1 2 0]
#   [0 3 4]
#   [5 0 6]
A :: vsparse.CSR := vsparse.from_dense(
    [1.0, 2.0, 0.0,
     0.0, 3.0, 4.0,
     5.0, 0.0, 6.0], 3, 3);

check("nnz", A.nnz == 6);
check("frobenius", close(vsparse.frobenius(A), 9.539392014169456, 1e-12));

x :: vsparse.Series := vsparse.Series([1.0, 2.0, 3.0], 3);
y :: vsparse.Series := vsparse.spmv(A, x);
check("spmv-row0", close(y.data[0], 5.0, 1e-12));
check("spmv-row1", close(y.data[1], 18.0, 1e-12));
check("spmv-row2", close(y.data[2], 23.0, 1e-12));

D :: Array<Float64> := vsparse.to_dense(A);
check("to-dense-0", close(D[0], 1.0, 1e-15));
check("to-dense-1", close(D[1], 2.0, 1e-15));
check("to-dense-4", close(D[4], 3.0, 1e-15));
check("to-dense-7", close(D[7], 0.0, 1e-15));
check("to-dense-8", close(D[8], 6.0, 1e-15));

T :: vsparse.CSR := vsparse.transpose(A);
xt :: vsparse.Series := vsparse.Series([1.0, 1.0, 1.0], 3);
yt := vsparse.spmv(T, xt);
check("transpose-row0", close(yt.data[0], 6.0, 1e-12));
check("transpose-row1", close(yt.data[1], 5.0, 1e-12));
check("transpose-row2", close(yt.data[2], 10.0, 1e-12));

I :: vsparse.CSR := vsparse.identity(3);
yi :: vsparse.Series := vsparse.spmv(I, x);
check("identity-0", close(yi.data[0], 1.0, 1e-15));
check("identity-1", close(yi.data[1], 2.0, 1e-15));
check("identity-2", close(yi.data[2], 3.0, 1e-15));

Di :: vsparse.CSR := vsparse.diagonal([2.0, 0.0, 4.0]);
yd := vsparse.spmv(Di, x);
check("diagonal-nnz", Di.nnz == 2);
check("diagonal-0", close(yd.data[0], 2.0, 1e-15));
check("diagonal-1", close(yd.data[1], 0.0, 1e-15));
check("diagonal-2", close(yd.data[2], 12.0, 1e-15));

Z :: vsparse.CSR := vsparse.zeros(3, 3);
yz := vsparse.spmv(Z, x);
check("zeros-0", close(yz.data[0], 0.0, 1e-15));
check("zeros-2", close(yz.data[2], 0.0, 1e-15));

# SPD system for CG:
#   [4 1 0] [x0]   [1]
#   [1 3 1] [x1] = [2]
#   [0 1 2] [x2]   [3]
S :: vsparse.CSR := vsparse.from_dense(
    [4.0, 1.0, 0.0,
     1.0, 3.0, 1.0,
     0.0, 1.0, 2.0], 3, 3);
b :: vsparse.Series := vsparse.Series([1.0, 2.0, 3.0], 3);
res :: vsparse.SolverResult := vsparse.cg(S, b, 1e-10, 100);
check("cg-converged", res.converged);
check("cg-residual", res.residual <= 1e-8);
check("cg-iterations", res.iterations > 0 && res.iterations <= 100);
check("cg-true-residual",
      vsparse.residual(S, b, vsparse.Series(res.x, 3)) <= 1e-8);

if failures[0] != 0 {
    out(string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
