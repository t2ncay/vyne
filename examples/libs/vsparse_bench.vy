# vsparse_bench.vy - SpMV throughput and CG convergence.
#
#   ./vynec --compile examples/libs/vsparse_bench.vy
#   ./examples/libs/vsparse_bench

use external "vsparse/vsparse.vy";
use native vcore;

out("vsparse benchmark");

# Tridiagonal SPD matrix: diag 2, off-diagonal -1.
fn tridiag(n :: Int64) -> vsparse.CSR {
    values :: Array<Float64> := [];
    col_idx :: Array<Int64> := [];
    row_ptr :: Array<Int64> := [];
    nnz :: Int64 := 0;
    through r :: 0..n-1 -> loop {
        row_ptr.push(nnz);
        if r > 0 {
            values.push(-1.0);
            col_idx.push(r - 1);
            nnz = nnz + 1;
        }
        values.push(2.0);
        col_idx.push(r);
        nnz = nnz + 1;
        if r + 1 < n {
            values.push(-1.0);
            col_idx.push(r + 1);
            nnz = nnz + 1;
        }
    };
    row_ptr.push(nnz);
    return vsparse.CSR(values, col_idx, row_ptr, n, n, nnz);
}

N :: Int64 := 10000;
A :: vsparse.CSR := tridiag(N);
out("nnz                : " + string(A.nnz));

ones :: Array<Float64> := [];
through i :: 0..N-1 -> loop { ones.push(1.0); };
x :: vsparse.Series := vsparse.Series(ones, N);

ITERS :: Int64 := 500;
sum :: Float64 := 0.0;
t0 :: Int64 := vcore.now_ns();
through it :: 0..ITERS-1 -> loop {
    y :: vsparse.Series := vsparse.spmv(A, x);
    sum = sum + y.data[it % N];
};
t1 :: Int64 := vcore.now_ns();
out("spmv 10k x 500     : " + string((t1 - t0) / 1000000) + " ms (sum=" + string(sum) + ")");

NC :: Int64 := 1000;
C :: vsparse.CSR := tridiag(NC);
b :: Array<Float64> := [];
through i :: 0..NC-1 -> loop { b.push(1.0); };
rhs :: vsparse.Series := vsparse.Series(b, NC);
t2 :: Int64 := vcore.now_ns();
r :: vsparse.SolverResult := vsparse.cg(C, rhs, 1e-8, 500);
t3 :: Int64 := vcore.now_ns();
out("cg 1k              : " + string((t3 - t2) / 1000000) + " ms, iter=" + string(r.iterations) + ", res=" + string(r.residual) + ", ok=" + string(r.converged));

out("done.");
