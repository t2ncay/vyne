# vlinalg/Constructors.vy — matrix factories and random initializers.

use "Types.vy";

ruleset {
    dynamic_casting
};

module vlinalg;
module vmath;

fn :: vlinalg zeros(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    n :: Int64 = rows * cols;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(0.0);
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg ones(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    n :: Int64 = rows * cols;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(1.0);
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg full(rows :: Int64, cols :: Int64, value :: Float64) -> vlinalg.Types.Matrix {
    n :: Int64 = rows * cols;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(value);
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg identity(n :: Int64) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    through r :: 0..n-1 -> loop {
        through c :: 0..n-1 -> loop {
            if r == c {
                out_data.push(1.0);
            } else {
                out_data.push(0.0);
            }
        };
    };
    return vlinalg.Types.Matrix(n, n, out_data);
}

fn :: vlinalg from_array(data :: Array) -> vlinalg.Types.Matrix {
    rows = data.size();
    if rows == 0 {
        return vlinalg.Types.Matrix(0, 0, []);
    }
    cols = data[0].size();
    out_data :: Array = [];
    through r :: 0..rows-1 -> loop {
        through c :: 0..cols-1 -> loop {
            out_data.push(float64(data[r][c]));
        };
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg from_flat(rows :: Int64, cols :: Int64, data :: Array) -> vlinalg.Types.Matrix {
    n :: Int64 = rows * cols;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(float64(data[i]));
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg random_uniform(rows :: Int64, cols :: Int64,
                             lo :: Float64, hi :: Float64) -> vlinalg.Types.Matrix {
    n :: Int64 = rows * cols;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(vmath.random_float(lo, hi));
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg xavier_init(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    limit :: Float64 = vmath.sqrt(6.0 / float64(rows + cols));
    return vlinalg.random_uniform(rows, cols, 0.0 - limit, limit);
}

fn :: vlinalg he_init(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    limit :: Float64 = vmath.sqrt(6.0 / float64(cols));
    return vlinalg.random_uniform(rows, cols, 0.0 - limit, limit);
}

fn :: vlinalg random_init(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    n :: Int64 = rows * cols;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        out_data.push(vmath.random_float(-0.5, 0.5));
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}