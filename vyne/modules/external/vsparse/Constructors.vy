# vsparse/Constructors.vy - CSR construction, conversion, and transpose.
#
# The direct interface constructor is zero-copy:
#   vsparse.CSR(values, col_idx, row_ptr, rows, cols, nnz)
# The helpers below copy or build fresh arrays.

use "Types.vy";

ruleset { dynamic_casting };

module vsparse;

fn :: vsparse zeros_f64(n :: Int64) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(0.0); };
    return data;
}

fn :: vsparse zeros_i64(n :: Int64) -> Array<Int64> {
    data :: Array<Int64> := [];
    through i :: 0..n-1 -> loop { data.push(0); };
    return data;
}

fn :: vsparse zeros(rows :: Int64, cols :: Int64) -> vsparse.CSR {
    if rows < 0 || cols < 0 { throw "vsparse.zeros: negative dimension"; }
    row_ptr :: Array<Int64> := vsparse.zeros_i64(rows + 1);
    values :: Array<Float64> := [];
    col_idx :: Array<Int64> := [];
    return vsparse.CSR(values, col_idx, row_ptr, rows, cols, 0);
}

fn :: vsparse identity(n :: Int64) -> vsparse.CSR {
    if n < 0 { throw "vsparse.identity: negative dimension"; }
    values :: Array<Float64> := [];
    col_idx :: Array<Int64> := [];
    row_ptr :: Array<Int64> := [];
    through r :: 0..n-1 -> loop {
        row_ptr.push(r);
        values.push(1.0);
        col_idx.push(r);
    };
    row_ptr.push(n);
    return vsparse.CSR(values, col_idx, row_ptr, n, n, n);
}

fn :: vsparse diagonal(v :: Array) -> vsparse.CSR {
    n :: Int64 := v.size();
    values :: Array<Float64> := [];
    col_idx :: Array<Int64> := [];
    row_ptr :: Array<Int64> := [];
    nnz :: Int64 := 0;
    through r :: 0..n-1 -> loop {
        row_ptr.push(nnz);
        x :: Float64 := v[r];
        if x != 0.0 {
            values.push(x);
            col_idx.push(r);
            nnz = nnz + 1;
        }
    };
    row_ptr.push(nnz);
    return vsparse.CSR(values, col_idx, row_ptr, n, n, nnz);
}

fn :: vsparse from_dense(dense :: Array, rows :: Int64,
                         cols :: Int64) -> vsparse.CSR {
    if rows < 0 || cols < 0 { throw "vsparse.from_dense: negative dimension"; }
    if dense.size() != rows * cols {
        throw "vsparse.from_dense: dense length does not match rows*cols";
    }
    values :: Array<Float64> := [];
    col_idx :: Array<Int64> := [];
    row_ptr :: Array<Int64> := [];
    nnz :: Int64 := 0;
    through r :: 0..rows-1 -> loop {
        row_ptr.push(nnz);
        through c :: 0..cols-1 -> loop {
            x :: Float64 := dense[r * cols + c];
            if x != 0.0 {
                values.push(x);
                col_idx.push(c);
                nnz = nnz + 1;
            }
        };
    };
    row_ptr.push(nnz);
    return vsparse.CSR(values, col_idx, row_ptr, rows, cols, nnz);
}

fn :: vsparse copy(A :: vsparse.CSR) -> vsparse.CSR {
    values :: Array<Float64> := [];
    col_idx :: Array<Int64> := [];
    row_ptr :: Array<Int64> := [];
    through k :: 0..A.nnz-1 -> loop {
        values.push(A.values[k]);
        col_idx.push(A.col_idx[k]);
    };
    through r :: 0..A.rows -> loop { row_ptr.push(A.row_ptr[r]); };
    return vsparse.CSR(values, col_idx, row_ptr, A.rows, A.cols, A.nnz);
}

fn :: vsparse to_dense(A :: vsparse.CSR) -> Array<Float64> {
    data :: Array<Float64> := vsparse.zeros_f64(A.rows * A.cols);
    through r :: 0..A.rows-1 -> loop {
        through k :: A.row_ptr[r]..A.row_ptr[r+1]-1 -> loop {
            data[r * A.cols + A.col_idx[k]] = A.values[k];
        };
    };
    return data;
}

fn :: vsparse transpose(A :: vsparse.CSR) -> vsparse.CSR {
    cols :: Int64 := A.cols;
    rows :: Int64 := A.rows;
    t_row_ptr :: Array<Int64> := vsparse.zeros_i64(cols + 1);
    through k :: 0..A.nnz-1 -> loop {
        t_row_ptr[A.col_idx[k] + 1] = t_row_ptr[A.col_idx[k] + 1] + 1;
    };
    through c :: 1..cols -> loop {
        t_row_ptr[c] = t_row_ptr[c] + t_row_ptr[c - 1];
    };
    next_pos :: Array<Int64> := vsparse.zeros_i64(cols);
    through c :: 0..cols-1 -> loop { next_pos[c] = t_row_ptr[c]; };
    t_values :: Array<Float64> := vsparse.zeros_f64(A.nnz);
    t_col_idx :: Array<Int64> := vsparse.zeros_i64(A.nnz);
    through r :: 0..A.rows-1 -> loop {
        through k :: A.row_ptr[r]..A.row_ptr[r + 1]-1 -> loop {
            c :: Int64 := A.col_idx[k];
            pos :: Int64 := next_pos[c];
            t_values[pos] = A.values[k];
            t_col_idx[pos] = r;
            next_pos[c] = pos + 1;
        };
    };
    return vsparse.CSR(t_values, t_col_idx, t_row_ptr, cols, rows, A.nnz);
}

fn :: vsparse row_count(A :: vsparse.CSR) -> Int64 { return A.rows; }
fn :: vsparse col_count(A :: vsparse.CSR) -> Int64 { return A.cols; }
fn :: vsparse nonzero_count(A :: vsparse.CSR) -> Int64 { return A.nnz; }
