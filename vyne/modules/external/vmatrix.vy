module vmatrix;

# Matrix module for vyne
# This module helps with matrix operations
# I will also add matrix multiplication when I have time

group c :: vmatrix {
    interface Matrix {
        row :: Int64,
        column :: Int64
    }
};

fn :: vmatrix add(a, b) {

    result :: Array = [];

    row_index = 0;

    through a -> collect {

        rowA = a[row_index];
        rowB = b[row_index];

        new_row = [];

        col_index = 0;

        through rowA -> collect {

            value = rowA[col_index] + rowB[col_index];

            new_row.push(value);

            col_index = col_index + 1;
        };

        result.push(new_row);

        row_index = row_index + 1;
    };

    return result;
}

fn :: vmatrix subtract(a, b) {
    
    result :: Array = [];

    row_index = 0;

    through a -> collect {

        rowA = a[row_index];
        rowB = b[row_index];

        new_row = [];

        col_index = 0;

        through rowA -> collect {

            value = rowA[col_index] - rowB[col_index];

            new_row.push(value);

            col_index = col_index + 1;
        };

        result.push(new_row);

        row_index = row_index + 1;
    };

    return result;
}

deploy vmatrix;