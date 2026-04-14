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

        insert_row(new_row :: Array) {
            self.row++;

            if new_row.size() != self.col {
                exit("Matrix Error: Inconsistent column size");
                return [];
            }

            new_data :: Array = self.data;
            new_data.push(new_row);
            return vlinalg.Types.Matrix(self.row, self.col, new_data);
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
    }
};

fn :: vlinalg add(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if (a.row != b.row) || (a.col != b.col) {
        out(vcolors.red("Matrix Error: Dimensions must match for addition."));
        return [];
    }

    result_data = [];

    through 0..a.row-1 -> loop {
        r = _;
        new_row = [];

        through 0..a.col-1 -> loop {
            c = _;
            
            sum = a.data[r][c] + b.data[r][c];
            new_row.push(sum);
        };

        result_data.push(new_row);
    };

    return vlinalg.Types.Matrix(a.row, a.col, result_data);
}

fn :: vlinalg subtract(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if (a.row != b.row) || (a.col != b.col) {
        out(vcolors.red("Matrix Error: Dimensions must match for subtraction."));
        return [];
    }

    result_data = [];

    through 0..a.row-1 -> loop {
        r = _;
        new_row = [];

        through 0..a.col-1 -> loop {
            c = _;
            diff = a.data[r][c] - b.data[r][c];
            new_row.push(diff);
        };

        result_data.push(new_row);
    };

    return vlinalg.Types.Matrix(a.row, a.col, result_data);
}

fn :: vlinalg multiply(a :: vlinalg.Types.Matrix, b :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    if a.col != b.row {
        out(vcolors.red("Matrix Error: Multiplication impossible (a.col != b.row)"));
        return [];
    }

    result_data = [];

    through r :: 0..a.row-1 -> loop {
        new_row = [];
        through c :: 0..b.col-1 -> loop {
            sum = 0.0;
            through k :: 0..a.col-1 -> loop {
                sum = sum + (a.data[r][k] * b.data[k][c]);
            };
            new_row.push(sum);
        };
        result_data.push(new_row);
    };

    return vlinalg.Types.Matrix(a.row, b.col, result_data);
}

fn :: vlinalg transpose(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    new_data = [];
    
    through c :: 0..m.col-1 -> loop {
        new_row = [];
        through r :: 0..m.row-1 -> loop {
            new_row.push(m.data[r][c]);
        };
        new_data.push(new_row);
    };

    return vlinalg.Types.Matrix(m.col, m.row, new_data);
}

fn :: vlinalg apply_sigmoid(m :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    res_data = through row_data :: m.data -> collect {
        through val :: row_data -> collect {
            vmath.sigmoid(val)
        }
    };
    # Vyne-da yeni Matrix obyektini belə qaytarırıq
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
        exit("Vlinalg Error: Hadamard dimensions mismatch");
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
            # 0.0 və 1.0 aralığında random rəqəm alırıq
            # Sonra onu MLP üçün uyğun olan kiçik aralığa salırıq
            (vmath.random(0.0, 1.0) - 0.5) * 1.0
        }
    };
    return vlinalg.Types.Matrix(rows, cols, res_data);
}
deploy vlinalg;
deploy vmath;
