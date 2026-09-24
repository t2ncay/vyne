ruleset {
    dynamic_casting
};

use lib "vcolors.vy";
module vlinalg;
module vmath;

group Types :: vlinalg {

    interface Matrix {
        row  :: Int64,
        col  :: Int64,
        data :: Array,

        # =============================================================
        # ORIGINAL (kept verbatim so existing code still compiles)
        # =============================================================
        insert_row(new_row :: Array) {
            self.row++;
            if new_row.size() != self.col {
                out(vcolors.red("Matrix Error: Inconsistent column size"));
                return [];
            }
            new_data :: Array = self.data;
            new_data.push(new_row);
            return vlinalg.Types.Matrix(self.row, self.col, new_data);
        }

        # =============================================================
        # SHAPE / STRUCTURE
        # =============================================================
        shape() -> Array {
            return [self.row, self.col];
        }

        size() -> Int64 {
            return self.row * self.col;
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
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(self.data[r][c]);
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        flatten() -> Array {
            flat :: Array = [];
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    flat.push(self.data[r][c]);
                };
            };
            return flat;
        }

        row_at(r :: Int64) -> Array {
            result :: Array = [];
            through c :: 0..self.col-1 -> loop {
                result.push(self.data[r][c]);
            };
            return result;
        }

        col_at(c :: Int64) -> Array {
            result :: Array = [];
            through r :: 0..self.row-1 -> loop {
                result.push(self.data[r][c]);
            };
            return result;
        }

        get(r :: Int64, c :: Int64) -> Float64 {
            return self.data[r][c];
        }

        # =============================================================
        # REDUCTIONS
        # =============================================================
        sum() -> Float64 {
            total :: Float64 = 0.0;
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    total = total + self.data[r][c];
                };
            };
            return total;
        }

        mean() -> Float64 {
            total :: Float64 = 0.0;
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    total = total + self.data[r][c];
                };
            };
            return total / float64(self.row * self.col);
        }

        minimum() -> Float64 {
            best :: Float64 = self.data[0][0];
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    if self.data[r][c] < best {
                        best = self.data[r][c];
                    }
                };
            };
            return best;
        }

        maximum() -> Float64 {
            best :: Float64 = self.data[0][0];
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    if self.data[r][c] > best {
                        best = self.data[r][c];
                    }
                };
            };
            return best;
        }

        trace() -> Float64 {
            total :: Float64 = 0.0;
            through i :: 0..self.row-1 -> loop {
                if i < self.col {
                    total = total + self.data[i][i];
                }
            };
            return total;
        }

        norm_fro() -> Float64 {
            acc :: Float64 = 0.0;
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    acc = acc + self.data[r][c] * self.data[r][c];
                };
            };
            return vmath.sqrt(acc);
        }

        argmax() -> Int64 {
            best_r = 0;
            best_c = 0;
            best   = self.data[0][0];
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    if self.data[r][c] > best {
                        best   = self.data[r][c];
                        best_r = r;
                        best_c = c;
                    }
                };
            };
            return best_r * self.col + best_c;
        }

        argmin() -> Int64 {
            best_r = 0;
            best_c = 0;
            best   = self.data[0][0];
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    if self.data[r][c] < best {
                        best   = self.data[r][c];
                        best_r = r;
                        best_c = c;
                    }
                };
            };
            return best_r * self.col + best_c;
        }

        # =============================================================
        # ELEMENT-WISE UNARY
        # =============================================================
        negate() {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(-self.data[r][c]);
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        apply_sigmoid() {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(vmath.sigmoid(self.data[r][c]));
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        apply_tanh() {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(vmath.tanh(self.data[r][c]));
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        apply_relu() {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(vmath.relu(self.data[r][c]));
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        apply_exp() {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(vmath.exp(self.data[r][c]));
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        apply_log() {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(vmath.log(self.data[r][c]));
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        # =============================================================
        # ELEMENT-WISE SCALAR
        # =============================================================
        add_scalar(s :: Float64) {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(self.data[r][c] + s);
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        sub_scalar(s :: Float64) {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(self.data[r][c] - s);
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        mul_scalar(s :: Float64) {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(self.data[r][c] * s);
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        div_scalar(s :: Float64) {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(self.data[r][c] / s);
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        clip(lo :: Float64, hi :: Float64) {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(vmath.clamp(self.data[r][c], lo, hi));
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        # =============================================================
        # SOFTMAX / NORMALIZATION
        # =============================================================
        softmax() {
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                row_max = self.data[r][0];
                through c :: 0..self.col-1 -> loop {
                    if self.data[r][c] > row_max {
                        row_max = self.data[r][c];
                    }
                };

                new_row :: Array = [];
                denom :: Float64 = 0.0;
                through c2 :: 0..self.col-1 -> loop {
                    ev = vmath.exp(self.data[r][c2] - row_max);
                    new_row.push(ev);
                    denom = denom + ev;
                };
                through c3 :: 0..self.col-1 -> loop {
                    new_row[c3] = new_row[c3] / denom;
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        normalize() {
            n = self.norm_fro();
            if n == 0.0 {
                n = 1.0;
            }
            out_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                new_row :: Array = [];
                through c :: 0..self.col-1 -> loop {
                    new_row.push(self.data[r][c] / n);
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.row, self.col, out_data);
        }

        # =============================================================
        # SHAPE MANIPULATION
        # =============================================================
        reshape(new_rows :: Int64, new_cols :: Int64) {
            if new_rows * new_cols != self.row * self.col {
                out(vcolors.red("Matrix Error: reshape size mismatch"));
                return self.copy();
            }

            flat_data :: Array = [];
            through r :: 0..self.row-1 -> loop {
                through c :: 0..self.col-1 -> loop {
                    flat_data.push(self.data[r][c]);
                };
            };

            out_data :: Array = [];
            idx = 0;
            through rr :: 0..new_rows-1 -> loop {
                new_row :: Array = [];
                through cc :: 0..new_cols-1 -> loop {
                    new_row.push(flat_data[idx]);
                    idx++;
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(new_rows, new_cols, out_data);
        }

        transposed() {
            out_data :: Array = [];
            through c :: 0..self.col-1 -> loop {
                new_row :: Array = [];
                through r :: 0..self.row-1 -> loop {
                    new_row.push(self.data[r][c]);
                };
                out_data.push(new_row);
            };
            return vlinalg.Types.Matrix(self.col, self.row, out_data);
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

# ============================================================================
# CONSTRUCTORS
# ============================================================================

fn :: vlinalg zeros(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    through r :: 0..rows-1 -> loop {
        new_row :: Array = [];
        through c :: 0..cols-1 -> loop {
            new_row.push(0.0);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg ones(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    through r :: 0..rows-1 -> loop {
        new_row :: Array = [];
        through c :: 0..cols-1 -> loop {
            new_row.push(1.0);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg full(rows :: Int64, cols :: Int64, value :: Float64) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    through r :: 0..rows-1 -> loop {
        new_row :: Array = [];
        through c :: 0..cols-1 -> loop {
            new_row.push(value);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg identity(n :: Int64) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    through r :: 0..n-1 -> loop {
        new_row :: Array = [];
        through c :: 0..n-1 -> loop {
            if r == c {
                new_row.push(1.0);
            } else {
                new_row.push(0.0);
            }
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(n, n, out_data);
}

fn :: vlinalg from_array(data :: Array) -> vlinalg.Types.Matrix {
    rows = data.size();
    if rows == 0 {
        return vlinalg.Types.Matrix(0, 0, []);
    }
    cols = data[0].size();
    return vlinalg.Types.Matrix(rows, cols, data);
}

fn :: vlinalg from_flat(rows :: Int64, cols :: Int64, data :: Array) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    idx = 0;
    through r :: 0..rows-1 -> loop {
        new_row :: Array = [];
        through c :: 0..cols-1 -> loop {
            new_row.push(data[idx]);
            idx++;
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

# ============================================================================
# RANDOM INITIALIZERS
# ============================================================================

fn :: vlinalg random_uniform(rows :: Int64, cols :: Int64, lo :: Float64, hi :: Float64) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    through r :: 0..rows-1 -> loop {
        new_row :: Array = [];
        through c :: 0..cols-1 -> loop {
            new_row.push(vmath.random_float(lo, hi));
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(rows, cols, out_data);
}

fn :: vlinalg xavier_init(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    limit = vmath.sqrt(6.0 / float64(rows + cols));
    return vlinalg.random_uniform(rows, cols, -limit, limit);
}

fn :: vlinalg he_init(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    limit = vmath.sqrt(6.0 / float64(cols));
    return vlinalg.random_uniform(rows, cols, -limit, limit);
}

# ============================================================================
# CORE OPS  (names / signatures kept identical for backwards compat)
# ============================================================================

fn :: vlinalg add(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if (a.row != b.row) || (a.col != b.col) {
        out(vcolors.red("Matrix Error: Dimensions must match for addition."));
        return [];
    }
    out_data :: Array = [];
    through r :: 0..a.row-1 -> loop {
        new_row :: Array = [];
        through c :: 0..a.col-1 -> loop {
            new_row.push(a.data[r][c] + b.data[r][c]);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(a.row, a.col, out_data);
}

fn :: vlinalg subtract(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if (a.row != b.row) || (a.col != b.col) {
        out(vcolors.red("Matrix Error: Dimensions must match for subtraction."));
        return [];
    }
    out_data :: Array = [];
    through r :: 0..a.row-1 -> loop {
        new_row :: Array = [];
        through c :: 0..a.col-1 -> loop {
            new_row.push(a.data[r][c] - b.data[r][c]);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(a.row, a.col, out_data);
}

fn :: vlinalg multiply(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.col != b.row {
        out(vcolors.red("Matrix Error: Multiplication impossible (a.col != b.row)"));
        return [];
    }
    out_data :: Array = [];
    through r :: 0..a.row-1 -> loop {
        new_row :: Array = [];
        through c :: 0..b.col-1 -> loop {
            acc :: Float64 = 0.0;
            through k :: 0..a.col-1 -> loop {
                acc = acc + (a.data[r][k] * b.data[k][c]);
            };
            new_row.push(acc);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(a.row, b.col, out_data);
}

fn :: vlinalg transpose(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    through c :: 0..m.col-1 -> loop {
        new_row :: Array = [];
        through r :: 0..m.row-1 -> loop {
            new_row.push(m.data[r][c]);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(m.col, m.row, out_data);
}

fn :: vlinalg apply_sigmoid(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through val :: row_data -> collect {
            vmath.sigmoid(val)
        }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg multiply_scalar(m :: vlinalg.Types.Matrix, scalar :: Float64) -> vlinalg.Types.Matrix {
    res_data = through row :: m.data -> collect {
        through val :: row -> collect { val * scalar }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg hadamard(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if (a.row != b.row) || (a.col != b.col) {
        out(vcolors.red("Vlinalg Error: Hadamard dimensions mismatch"));
        return [];
    }
    res_data = through r :: 0..a.row-1 -> collect {
        through c :: 0..a.col-1 -> collect {
            a.data[r][c] * b.data[r][c]
        }
    };
    return vlinalg.Types.Matrix(a.row, a.col, res_data);
}

fn :: vlinalg sigmoid_prime(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through a :: row_data -> collect {
            a * (1.0 - a)
        }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg add_bias(m :: vlinalg.Types.Matrix, b :: Array) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through c :: 0..m.col-1 -> collect {
            row_data[c] + b[c]
        }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg random_init(rows :: Int64, cols :: Int64) -> vlinalg.Types.Matrix {
    res_data = through r :: 0..rows-1 -> collect {
        through c :: 0..cols-1 -> collect {
            (vmath.random(0.0, 1.0) - 0.5) * 1.0
        }
    };
    return vlinalg.Types.Matrix(rows, cols, res_data);
}

# ============================================================================
# NEW: ACTIVATIONS & DERIVATIVES (top-level versions)
# ============================================================================

fn :: vlinalg apply_tanh(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through val :: row_data -> collect { vmath.tanh(val) }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg apply_relu(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through val :: row_data -> collect { vmath.relu(val) }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg apply_exp(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through val :: row_data -> collect { vmath.exp(val) }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg apply_log(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through val :: row_data -> collect { vmath.log(val) }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg tanh_prime(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    # Takes tanh(x) and returns the derivative 1 - tanh(x)^2.
    res_data = through row_data :: m.data -> collect {
        through v :: row_data -> collect {
            1.0 - v * v
        }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg relu_prime(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through v :: row_data -> collect {
            if v > 0.0 { 1.0 } else { 0.0 }
        }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg add_scalar(m :: vlinalg.Types.Matrix, scalar :: Float64) -> vlinalg.Types.Matrix {
    res_data = through row :: m.data -> collect {
        through val :: row -> collect { val + scalar }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

fn :: vlinalg clip(m :: vlinalg.Types.Matrix, lo :: Float64, hi :: Float64) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through v :: row_data -> collect {
            vmath.clamp(v, lo, hi)
        }
    };
    return vlinalg.Types.Matrix(m.row, m.col, res_data);
}

# ============================================================================
# NEW: REDUCTIONS (top-level versions)
# ============================================================================

fn :: vlinalg sum(m :: vlinalg.Types.Matrix) -> Float64 {
    total :: Float64 = 0.0;
    through r :: 0..m.row-1 -> loop {
        through c :: 0..m.col-1 -> loop {
            total = total + m.data[r][c];
        };
    };
    return total;
}

fn :: vlinalg mean(m :: vlinalg.Types.Matrix) -> Float64 {
    total :: Float64 = 0.0;
    through r :: 0..m.row-1 -> loop {
        through c :: 0..m.col-1 -> loop {
            total = total + m.data[r][c];
        };
    };
    return total / float64(m.row * m.col);
}

fn :: vlinalg minimum(m :: vlinalg.Types.Matrix) -> Float64 {
    best :: Float64 = m.data[0][0];
    through r :: 0..m.row-1 -> loop {
        through c :: 0..m.col-1 -> loop {
            if m.data[r][c] < best {
                best = m.data[r][c];
            }
        };
    };
    return best;
}

fn :: vlinalg maximum(m :: vlinalg.Types.Matrix) -> Float64 {
    best :: Float64 = m.data[0][0];
    through r :: 0..m.row-1 -> loop {
        through c :: 0..m.col-1 -> loop {
            if m.data[r][c] > best {
                best = m.data[r][c];
            }
        };
    };
    return best;
}

fn :: vlinalg norm_fro(m :: vlinalg.Types.Matrix) -> Float64 {
    acc :: Float64 = 0.0;
    through r :: 0..m.row-1 -> loop {
        through c :: 0..m.col-1 -> loop {
            acc = acc + m.data[r][c] * m.data[r][c];
        };
    };
    return vmath.sqrt(acc);
}

fn :: vlinalg trace(m :: vlinalg.Types.Matrix) -> Float64 {
    total :: Float64 = 0.0;
    through i :: 0..m.row-1 -> loop {
        if i < m.col {
            total = total + m.data[i][i];
        }
    };
    return total;
}

# ============================================================================
# NEW: SOFTMAX / NORMALIZE (top-level)
# ============================================================================

fn :: vlinalg softmax(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    out_data :: Array = [];
    through r :: 0..m.row-1 -> loop {
        row_max = m.data[r][0];
        through c :: 0..m.col-1 -> loop {
            if m.data[r][c] > row_max {
                row_max = m.data[r][c];
            }
        };

        new_row :: Array = [];
        denom :: Float64 = 0.0;
        through c2 :: 0..m.col-1 -> loop {
            ev = vmath.exp(m.data[r][c2] - row_max);
            new_row.push(ev);
            denom = denom + ev;
        };
        through c3 :: 0..m.col-1 -> loop {
            new_row[c3] = new_row[c3] / denom;
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(m.row, m.col, out_data);
}

fn :: vlinalg normalize(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n = vlinalg.norm_fro(m);
    if n == 0.0 {
        n = 1.0;
    }
    return vlinalg.multiply_scalar(m, 1.0 / n);
}

# ============================================================================
# NEW: LOSSES
# ============================================================================

fn :: vlinalg mse(pred :: vlinalg.Types.Matrix, target :: vlinalg.Types.Matrix) -> Float64 {
    total :: Float64 = 0.0;
    through r :: 0..pred.row-1 -> loop {
        through c :: 0..pred.col-1 -> loop {
            d = pred.data[r][c] - target.data[r][c];
            total = total + d * d;
        };
    };
    return total / float64(pred.row * pred.col);
}

fn :: vlinalg mse_prime(pred :: vlinalg.Types.Matrix, target :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    n = float64(pred.row * pred.col);
    res_data = through r :: 0..pred.row-1 -> collect {
        through c :: 0..pred.col-1 -> collect {
            2.0 * (pred.data[r][c] - target.data[r][c]) / n
        }
    };
    return vlinalg.Types.Matrix(pred.row, pred.col, res_data);
}

fn :: vlinalg cross_entropy(pred, target) -> Float64 {
    eps = 0.000000001;
    total :: Float64 = 0.0;
    through r :: 0..pred.row-1 -> loop {
        through c :: 0..pred.col-1 -> loop {
            p = vmath.clamp(pred.data[r][c], eps, 1.0 - eps);
            y = target.data[r][c];
            total = total - (y * vmath.log(p) + (1.0 - y) * vmath.log(1.0 - p));
        };
    };
    return total / float64(pred.row);
}

fn :: vlinalg cross_entropy_prime(pred, target) -> vlinalg.Types.Matrix {
    res_data = through r :: 0..pred.row-1 -> collect {
        through c :: 0..pred.col-1 -> collect {
            pred.data[r][c] - target.data[r][c]
        }
    };
    return vlinalg.Types.Matrix(pred.row, pred.col, res_data);
}

# ============================================================================
# NEW: VECTOR / MATRIX PRODUCTS
# ============================================================================

fn :: vlinalg dot(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> Float64 {
    total :: Float64 = 0.0;
    if a.row == 1 && b.row == 1 {
        through c :: 0..a.col-1 -> loop {
            total = total + a.data[0][c] * b.data[0][c];
        };
    } else if a.col == 1 && b.col == 1 {
        through r :: 0..a.row-1 -> loop {
            total = total + a.data[r][0] * b.data[r][0];
        };
    } else if a.row == 1 && b.col == 1 {
        through c :: 0..a.col-1 -> loop {
            total = total + a.data[0][c] * b.data[c][0];
        };
    } else if a.col == 1 && b.row == 1 {
        through r :: 0..a.row-1 -> loop {
            total = total + a.data[r][0] * b.data[0][r];
        };
    } else {
        out(vcolors.red("Matrix Error: dot() requires vectors"));
    }
    return total;
}

fn :: vlinalg outer(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    # Takes two column vectors (Nx1, Mx1) and returns NxM.
    n = a.row;
    m = b.row;
    out_data :: Array = [];
    through i :: 0..n-1 -> loop {
        new_row :: Array = [];
        through j :: 0..m-1 -> loop {
            new_row.push(a.data[i][0] * b.data[j][0]);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(n, m, out_data);
}

# ============================================================================
# NEW: STACKING / CONCATENATION
# ============================================================================

fn :: vlinalg vstack(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.col != b.col {
        out(vcolors.red("Matrix Error: vstack column mismatch"));
        return [];
    }
    out_data :: Array = [];
    through r :: 0..a.row-1 -> loop {
        new_row :: Array = [];
        through c :: 0..a.col-1 -> loop {
            new_row.push(a.data[r][c]);
        };
        out_data.push(new_row);
    };
    through r2 :: 0..b.row-1 -> loop {
        new_row :: Array = [];
        through c2 :: 0..b.col-1 -> loop {
            new_row.push(b.data[r2][c2]);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(a.row + b.row, a.col, out_data);
}

fn :: vlinalg hstack(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.row != b.row {
        out(vcolors.red("Matrix Error: hstack row mismatch"));
        return [];
    }
    out_data :: Array = [];
    through r :: 0..a.row-1 -> loop {
        new_row :: Array = [];
        through c :: 0..a.col-1 -> loop {
            new_row.push(a.data[r][c]);
        };
        through c2 :: 0..b.col-1 -> loop {
            new_row.push(b.data[r][c2]);
        };
        out_data.push(new_row);
    };
    return vlinalg.Types.Matrix(a.row, a.col + b.col, out_data);
}

# ============================================================================
# DEPLOY
# ============================================================================

deploy vlinalg;
deploy vmath;