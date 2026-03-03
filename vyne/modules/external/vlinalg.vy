use lib "vcolors.vy";
module vlinalg;
module vmath;

### TODO FIX RECURSIVE TYPE CHECKING INSIDE INTERFACES

group Types :: vlinalg {
    
    interface Matrix {
        row  :: Int64,
        col  :: Int64,
        data :: Array,

        insert_row(new_row :: Array) {
            self.row = self.row + 1;
            # fix postfix node self variable resolution in scope

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

deploy vlinalg;
deploy vmath;
