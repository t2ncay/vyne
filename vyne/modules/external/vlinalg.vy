use extern "vcolors.vy";
module vlinalg;

group Types :: vlinalg {
    interface Matrix {
        row :: Int64,
        col :: Int64,
        data :: Array
    }

    interface Vector {
        x :: Int64,
        y :: Int64,

        slope(){
            return self.y / self.x;
        }
    }
};

fn :: vlinalg add(a :: Types.Matrix, b :: Types.Matrix) -> Types.Matrix {
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

    return Types.Matrix(a.row, a.col, result_data);
}

fn :: vlinalg subtract(a :: Types.Matrix, b :: Types.Matrix) -> Types.Matrix {
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

    return Types.Matrix(a.row, a.col, result_data);
}

deploy vlinalg;