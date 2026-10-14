# vlin/Types.vy — the Matrix interface.
#
# `data :: Array<Float64>` (not `Array`) is load-bearing: it tells the
# emitter to lower `m.data` reads to a `VyneArray_f64` local, so
# passing `m.data` to a kernel function matches the native array ABI.

use external "vcolors.vy";

ruleset { dynamic_casting };

module vlin;
module vmath;

group Types :: vlin {

    interface Matrix {
        row  :: Int64,
        col  :: Int64,
        data :: Array<Float64>,

        # -- shape -----------------------------------------------------
        shape() -> Array { return [self.row, self.col]; }
        size() -> Int64 { return self.row * self.col; }
        is_square() -> Bool { return self.row == self.col; }
        is_vector() -> Bool {
            if self.row == 1 { return true; }
            if self.col == 1 { return true; }
            return false;
        }

        # -- element read (unchecked; use with known-good indices) -----
        get(r :: Int64, c :: Int64) -> Float64 {
            return self.data[r * self.col + c];
        }

        # -- rows / cols as fresh typed arrays ------------------------
        row_at(r :: Int64) -> Array<Float64> {
            c0 :: Int64 := self.col;
            result :: Array<Float64> := [];
            through c :: 0..c0-1 -> loop {
                result.push(self.data[r * c0 + c]);
            };
            return result;
        }

        col_at(c :: Int64) -> Array<Float64> {
            r0 :: Int64 := self.row;
            c0 :: Int64 := self.col;
            result :: Array<Float64> := [];
            through r :: 0..r0-1 -> loop {
                result.push(self.data[r * c0 + c]);
            };
            return result;
        }

        # -- copy ------------------------------------------------------
        copy() {
            r :: Int64 := self.row;
            c :: Int64 := self.col;
            n :: Int64 := r * c;
            output_data :: Array<Float64> := [];
            through i :: 0..n-1 -> loop { output_data.push(self.data[i]); };
            return vlin.Types.Matrix(r, c, output_data);
        }

        flatten() -> Array<Float64> {
            return self.data;
        }

        # -- reductions delegated to top-level fns --------------------
        sum()      -> Float64 { return vlin.sum(self); }
        mean()     -> Float64 { return vlin.mean(self); }
        minimum()  -> Float64 { return vlin.minimum(self); }
        maximum()  -> Float64 { return vlin.maximum(self); }
        trace()    -> Float64 { return vlin.trace(self); }
        norm_fro() -> Float64 { return vlin.norm_fro(self); }
        argmax()   -> Int64   { return vlin.argmax(self); }
        argmin()   -> Int64   { return vlin.argmin(self); }

        # -- element-wise, returns fresh Matrix -----------------------
        negate() {
            r :: Int64 := self.row;
            c :: Int64 := self.col;
            n :: Int64 := r * c;
            output_data :: Array<Float64> := [];
            through i :: 0..n-1 -> loop { output_data.push(0.0 - self.data[i]); };
            return vlin.Types.Matrix(r, c, output_data);
        }

        add_scalar(s :: Float64) {
            r :: Int64 := self.row;
            c :: Int64 := self.col;
            n :: Int64 := r * c;
            output_data :: Array<Float64> := [];
            through i :: 0..n-1 -> loop { output_data.push(self.data[i] + s); };
            return vlin.Types.Matrix(r, c, output_data);
        }

        mul_scalar(s :: Float64) {
            r :: Int64 := self.row;
            c :: Int64 := self.col;
            n :: Int64 := r * c;
            output_data :: Array<Float64> := [];
            through i :: 0..n-1 -> loop { output_data.push(self.data[i] * s); };
            return vlin.Types.Matrix(r, c, output_data);
        }

        # -- reshape / transpose --------------------------------------
        reshape(new_rows :: Int64, new_cols :: Int64) {
            r :: Int64 := self.row;
            c :: Int64 := self.col;
            if new_rows * new_cols != r * c {
                out(vcolors.red("Matrix Error: reshape size mismatch"));
                return self.copy();
            }
            n :: Int64 := r * c;
            output_data :: Array<Float64> := [];
            through i :: 0..n-1 -> loop { output_data.push(self.data[i]); };
            return vlin.Types.Matrix(new_rows, new_cols, output_data);
        }

        transposed() {
            r :: Int64 := self.row;
            c :: Int64 := self.col;
            n :: Int64 := r * c;
            output_data :: Array<Float64> := [];
            through i :: 0..n-1 -> loop { output_data.push(0.0); };
            vlin.k_transpose(output_data, self.data, r, c);
            return vlin.Types.Matrix(c, r, output_data);
        }
    }

    interface Vector {
        x :: Int64,
        y :: Int64,

        magnitude() -> Float64 {
            return vmath.sqrt(self.x * self.x + self.y * self.y);
        }
        slope() -> Float64 { return self.y / self.x; }
        dot(other :: Types.Vector) -> Int64 {
            return self.x * other.x + self.y * other.y;
        }
        cross_product(other :: Types.Vector) -> Int64 {
            return self.x * other.y - self.y * other.x;
        }
    }
};
