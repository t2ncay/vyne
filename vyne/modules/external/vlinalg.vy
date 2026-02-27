use extern "vcolors.vy";
module vlinalg;

# Matrix module for vyne
# This module helps with matrix operations
# I will also add matrix multiplication when I have time


# Matrix module for vyne
# Standardizes matrix operations using the Matrix interface

interface Matrix :: vlinalg {
    row :: Int64,
    col :: Int64,
    data :: Array
}

fn :: vlinalg add(a :: Matrix, b :: Matrix) -> Matrix {
    if (a.row != b.row) || (a.col != b.col) {
        out(vcolors.red("Matrix Error: Dimensions must match for addition."));
        return [];
    }

    result_data = [];

    through 0..a.row -> loop {
        r = _;
        new_row = [];

        through 0..a.col -> loop {
            c = _;
            
            sum = a.data[r][c] + b.data[r][c];
            new_row.push(sum);
        };

        result_data.push(new_row);
    };

    # 4. In a future update, we can return a Struct instance here. 
    return result_data;
}

fn :: vlinalg subtract(a :: Matrix, b :: Matrix) -> Matrix {
    if (a.row != b.row) || (a.col != b.col) {
        out(vcolors.red("Matrix Error: Dimensions must match for addition."));
        return [];
    }

    result_data = [];

    through 0..a.row -> loop {
        r = _;
        new_row = [];

        through 0..a.col -> loop {
            c = _;
            diff = a.data[r][c] - b.data[r][c];
            new_row.push(diff);
        };

        result_data.push(new_row);
    };

    return result_data;
}

deploy vlinalg;