# vlinalg/Types.vy — core matrix/vector interfaces.
#
# Matrix.data is stored FLAT: data[r * col + c]. The Array<Float64>
# annotation is load-bearing — it tells the C backend to unbox the
# field into a VyneArray_f64 on every read, so self.data[i] compiles
# to a direct double load.

use lib "vcolors.vy";

ruleset {
    dynamic_casting
};

module vlinalg;
module vmath;

group Types :: vlinalg {

    interface Matrix {
        row  :: Int64,
        col  :: Int64,
        data :: Array<Float64>,

        # -----------------------------------------------------------
        # SHAPE / STRUCTURE
        # -----------------------------------------------------------
        shape() -> Array {
            return [self.row, self.col];
        }

        size() -> Int64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            return r * c;
        }

        is_square() -> Bool {
            return self.row == self.col;
        }

        is_vector() -> Bool {
            if self.row == 1 { return true; }
            if self.col == 1 { return true; }
            return false;
        }

        copy() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(self.data[i]);
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        flatten() -> Array {
            return self.data;
        }

        row_at(r :: Int64) -> Array {
            c0 :: Int64 = self.col;
            result :: Array = [];
            through c :: 0..c0-1 -> loop {
                result.push(self.data[r * c0 + c]);
            };
            return result;
        }

        col_at(c :: Int64) -> Array {
            r0 :: Int64 = self.row;
            c0 :: Int64 = self.col;
            result :: Array = [];
            through r :: 0..r0-1 -> loop {
                result.push(self.data[r * c0 + c]);
            };
            return result;
        }

        get(r :: Int64, c :: Int64) -> Float64 {
            return self.data[r * self.col + c];
        }

        insert_row(new_row :: Array) {
            if new_row.size() != self.col {
                out(vcolors.red("Matrix Error: Inconsistent column size"));
                return vlinalg.Types.Matrix(0, 0, []);
            }
            r0 :: Int64 = self.row;
            c0 :: Int64 = self.col;
            n  :: Int64 = r0 * c0;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(self.data[i]);
            };
            through j :: 0..c0-1 -> loop {
                out_data.push(new_row[j]);
            };
            return vlinalg.Types.Matrix(r0 + 1, c0, out_data);
        }

        # -----------------------------------------------------------
        # REDUCTIONS
        # -----------------------------------------------------------
        sum() -> Float64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            total :: Float64 = 0.0;
            through i :: 0..n-1 -> loop {
                total = total + self.data[i];
            };
            return total;
        }

        mean() -> Float64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            if n == 0 { return 0.0; }
            total :: Float64 = 0.0;
            through i :: 0..n-1 -> loop {
                total = total + self.data[i];
            };
            return total / float64(n);
        }

        minimum() -> Float64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            if n == 0 { return 0.0; }
            best :: Float64 = self.data[0];
            through i :: 1..n-1 -> loop {
                v :: Float64 = self.data[i];
                if v < best { best = v; }
            };
            return best;
        }

        maximum() -> Float64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            if n == 0 { return 0.0; }
            best :: Float64 = self.data[0];
            through i :: 1..n-1 -> loop {
                v :: Float64 = self.data[i];
                if v > best { best = v; }
            };
            return best;
        }

        trace() -> Float64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            m :: Int64 = r;
            if c < m { m = c; }
            total :: Float64 = 0.0;
            through i :: 0..m-1 -> loop {
                total = total + self.data[i * c + i];
            };
            return total;
        }

        norm_fro() -> Float64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            acc :: Float64 = 0.0;
            through i :: 0..n-1 -> loop {
                x :: Float64 = self.data[i];
                acc = acc + x * x;
            };
            return vmath.sqrt(acc);
        }

        argmax() -> Int64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            if n == 0 { return 0; }
            best   :: Float64 = self.data[0];
            best_i :: Int64   = 0;
            through i :: 1..n-1 -> loop {
                v :: Float64 = self.data[i];
                if v > best {
                    best   = v;
                    best_i = i;
                }
            };
            return best_i;
        }

        argmin() -> Int64 {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            if n == 0 { return 0; }
            best   :: Float64 = self.data[0];
            best_i :: Int64   = 0;
            through i :: 1..n-1 -> loop {
                v :: Float64 = self.data[i];
                if v < best {
                    best   = v;
                    best_i = i;
                }
            };
            return best_i;
        }

        # -----------------------------------------------------------
        # ELEMENT-WISE UNARY
        # -----------------------------------------------------------
        negate() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(-self.data[i]);
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        apply_sigmoid() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(vmath.sigmoid(self.data[i]));
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        apply_tanh() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(vmath.tanh(self.data[i]));
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        apply_relu() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(vmath.relu(self.data[i]));
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        apply_exp() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(vmath.exp(self.data[i]));
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        apply_log() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(vmath.log(self.data[i]));
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        # -----------------------------------------------------------
        # ELEMENT-WISE SCALAR
        # -----------------------------------------------------------
        add_scalar(s :: Float64) {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(self.data[i] + s);
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        sub_scalar(s :: Float64) {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(self.data[i] - s);
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        mul_scalar(s :: Float64) {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(self.data[i] * s);
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        div_scalar(s :: Float64) {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(self.data[i] / s);
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        clip(lo :: Float64, hi :: Float64) {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(vmath.clamp(self.data[i], lo, hi));
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        # -----------------------------------------------------------
        # SOFTMAX / NORMALIZATION
        # -----------------------------------------------------------
        softmax() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            out_data :: Array = [];
            through i :: 0..r-1 -> loop {
                base :: Int64 = i * c;

                row_max :: Float64 = self.data[base];
                through j :: 1..c-1 -> loop {
                    v :: Float64 = self.data[base + j];
                    if v > row_max { row_max = v; }
                };

                start_idx :: Int64 = out_data.size();
                denom :: Float64 = 0.0;
                through j :: 0..c-1 -> loop {
                    e :: Float64 = vmath.exp(self.data[base + j] - row_max);
                    out_data.push(e);
                    denom = denom + e;
                };

                through j :: 0..c-1 -> loop {
                    out_data[start_idx + j] = out_data[start_idx + j] / denom;
                };
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        normalize() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            n :: Int64 = r * c;

            nrm :: Float64 = self.norm_fro();
            if nrm == 0.0 { nrm = 1.0; }

            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(self.data[i] / nrm);
            };
            return vlinalg.Types.Matrix(r, c, out_data);
        }

        # -----------------------------------------------------------
        # SHAPE MANIPULATION
        # -----------------------------------------------------------
        reshape(new_rows :: Int64, new_cols :: Int64) {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            if new_rows * new_cols != r * c {
                out(vcolors.red("Matrix Error: reshape size mismatch"));
                return self.copy();
            }
            n :: Int64 = r * c;
            out_data :: Array = [];
            through i :: 0..n-1 -> loop {
                out_data.push(self.data[i]);
            };
            return vlinalg.Types.Matrix(new_rows, new_cols, out_data);
        }

        transposed() {
            r :: Int64 = self.row;
            c :: Int64 = self.col;
            out_data :: Array = [];
            through i :: 0..c-1 -> loop {
                through j :: 0..r-1 -> loop {
                    out_data.push(self.data[j * c + i]);
                };
            };
            return vlinalg.Types.Matrix(c, r, out_data);
        }
    }

    interface Vector {
        x :: Int64,
        y :: Int64,

        magnitude() -> Float64 {
            return vmath.sqrt(self.x * self.x + self.y * self.y);
        }

        slope() -> Float64 {
            return self.y / self.x;
        }

        cross_product(other :: Types.Vector) -> Int64 {
            return self.x * other.y - self.y * other.x;
        }

        dot(other :: Types.Vector) -> Int64 {
            return self.x * other.x + self.y * other.y;
        }
    }
};