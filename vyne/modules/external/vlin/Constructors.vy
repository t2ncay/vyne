# vlin/Constructors.vy — fresh allocations and random initializers.

use "Types.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vlin;
use native vmath;
# ---- raw typed-array allocators (ABI-returning) ---------------------------

fn :: vlin zeros_f64(n :: Int64) -> Array<Float64> {
    output :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { output.push(0.0); };
    return output;
}

fn :: vlin ones_f64(n :: Int64) -> Array<Float64> {
    output :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { output.push(1.0); };
    return output;
}

fn :: vlin fill_f64(n :: Int64, v :: Float64) -> Array<Float64> {
    output :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { output.push(v); };
    return output;
}

fn :: vlin clone_f64(a :: Array<Float64>, n :: Int64) -> Array<Float64> {
    output :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { output.push(a[i]); };
    return output;
}

# ---- Matrix factories -----------------------------------------------------

fn :: vlin zeros(rows :: Int64, cols :: Int64) -> vlin.Types.Matrix {
    n :: Int64 := rows * cols;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    return vlin.Types.Matrix(rows, cols, output_data);
}

fn :: vlin ones(rows :: Int64, cols :: Int64) -> vlin.Types.Matrix {
    n :: Int64 := rows * cols;
    output_data :: Array<Float64> := vlin.ones_f64(n);
    return vlin.Types.Matrix(rows, cols, output_data);
}

fn :: vlin full(rows :: Int64, cols :: Int64, value :: Float64) -> vlin.Types.Matrix {
    n :: Int64 := rows * cols;
    output_data :: Array<Float64> := vlin.fill_f64(n, value);
    return vlin.Types.Matrix(rows, cols, output_data);
}

fn :: vlin identity(n :: Int64) -> vlin.Types.Matrix {
    output_data :: Array<Float64> := vlin.zeros_f64(n * n);
    through i :: 0..n-1 -> loop { output_data[i * n + i] = 1.0; };
    return vlin.Types.Matrix(n, n, output_data);
}

fn :: vlin from_array(data :: Array) -> vlin.Types.Matrix {
    rows :: Int64 := data.size();
    if rows == 0 {
        return vlin.Types.Matrix(0, 0, vlin.zeros_f64(0));
    }
    cols :: Int64 := data[0].size();
    n :: Int64 := rows * cols;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    through r :: 0..rows-1 -> loop {
        through c :: 0..cols-1 -> loop {
            output_data[r * cols + c] = float64(data[r][c]);
        };
    };
    return vlin.Types.Matrix(rows, cols, output_data);
}

fn :: vlin from_flat(rows :: Int64, cols :: Int64, data :: Array) -> vlin.Types.Matrix {
    n :: Int64 := rows * cols;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop { output_data[i] = float64(data[i]); };
    return vlin.Types.Matrix(rows, cols, output_data);
}

# ---- Random initializers --------------------------------------------------

fn :: vlin random_uniform(rows :: Int64, cols :: Int64,
                          lo :: Float64, hi :: Float64) -> vlin.Types.Matrix {
    n :: Int64 := rows * cols;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        output_data[i] = vmath.random_float(lo, hi);
    };
    return vlin.Types.Matrix(rows, cols, output_data);
}

fn :: vlin xavier_init(rows :: Int64, cols :: Int64) -> vlin.Types.Matrix {
    limit :: Float64 := vmath.sqrt(6.0 / float64(rows + cols));
    return vlin.random_uniform(rows, cols, 0.0 - limit, limit);
}

fn :: vlin he_init(rows :: Int64, cols :: Int64) -> vlin.Types.Matrix {
    limit :: Float64 := vmath.sqrt(6.0 / float64(cols));
    return vlin.random_uniform(rows, cols, 0.0 - limit, limit);
}

fn :: vlin random_init(rows :: Int64, cols :: Int64) -> vlin.Types.Matrix {
    n :: Int64 := rows * cols;
    output_data :: Array<Float64> := vlin.zeros_f64(n);
    through i :: 0..n-1 -> loop {
        output_data[i] = vmath.random_float(-0.5, 0.5);
    };
    return vlin.Types.Matrix(rows, cols, output_data);
}
