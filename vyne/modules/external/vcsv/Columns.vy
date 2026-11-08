# vcsv/Columns.vy - cell, row, column, and typed-column access.

use "Types.vy";

ruleset { dynamic_casting };

module vcsv;

fn :: vcsv cell(t :: vcsv.Table, r :: Int64, c :: Int64) -> String {
    if r < 0 || r >= t.n_rows || c < 0 || c >= t.n_cols {
        throw "vcsv.cell: index out of bounds";
    }
    return t.cells[r * t.n_cols + c];
}

fn :: vcsv header(t :: vcsv.Table, c :: Int64) -> String {
    if c < 0 || c >= t.n_cols || t.headers.size() == 0 {
        throw "vcsv.header: no header at that column";
    }
    return t.headers[c];
}

fn :: vcsv has_headers(t :: vcsv.Table) -> Bool {
    return t.headers.size() > 0;
}

fn :: vcsv shape(t :: vcsv.Table) -> Array {
    return [t.n_rows, t.n_cols];
}

fn :: vcsv column_names(t :: vcsv.Table) -> Array {
    return t.headers;
}

fn :: vcsv row(t :: vcsv.Table, r :: Int64) -> Array {
    if r < 0 || r >= t.n_rows { throw "vcsv.row: index out of bounds"; }
    result :: Array := [];
    through c :: 0..t.n_cols-1 -> loop {
        result.push(t.cells[r * t.n_cols + c]);
    };
    return result;
}

fn :: vcsv column(t :: vcsv.Table, c :: Int64) -> Array {
    if c < 0 || c >= t.n_cols { throw "vcsv.column: index out of bounds"; }
    result :: Array := [];
    through r :: 0..t.n_rows-1 -> loop {
        result.push(t.cells[r * t.n_cols + c]);
    };
    return result;
}

fn :: vcsv column_f64(t :: vcsv.Table, c :: Int64) -> Array<Float64> {
    if c < 0 || c >= t.n_cols { throw "vcsv.column_f64: index out of bounds"; }
    result :: Array<Float64> := [];
    through r :: 0..t.n_rows-1 -> loop {
        result.push(float64(t.cells[r * t.n_cols + c]));
    };
    return result;
}

fn :: vcsv column_i64(t :: vcsv.Table, c :: Int64) -> Array<Int64> {
    if c < 0 || c >= t.n_cols { throw "vcsv.column_i64: index out of bounds"; }
    result :: Array<Int64> := [];
    through r :: 0..t.n_rows-1 -> loop {
        result.push(int64(t.cells[r * t.n_cols + c]));
    };
    return result;
}

fn :: vcsv select_columns(t :: vcsv.Table, idx :: Array) -> vcsv.Table {
    n :: Int64 := idx.size();
    headers2 :: Array := [];
    if t.headers.size() > 0 {
        through c :: 0..n-1 -> loop {
            headers2.push(t.headers[int64(idx[c])]);
        };
    }
    cells2 :: Array := [];
    through r :: 0..t.n_rows-1 -> loop {
        through c2 :: 0..n-1 -> loop {
            ci :: Int64 := int64(idx[c2]);
            if ci < 0 || ci >= t.n_cols {
                throw "vcsv.select_columns: column index out of bounds";
            }
            cells2.push(t.cells[r * t.n_cols + ci]);
        };
    };
    return vcsv.Table(headers2, cells2, t.n_rows, n);
}
