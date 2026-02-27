use extern "vcolors.vy";
module vlinalg;

# IMPORTANT NOTE : THIS LIBRARY INCLUDES
# A SET OF FEATURES THAT VYNE CURRENTLY DOES NOT SUPPORT
# ALL IMPLEMENTED FEATURES WILL BE RELEASED IN THE FUTURE

# IN ORDER TO WORK : IMPLEMENT INDEX ASSIGNMENTS

interface Matrix {
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

    return Matrix(a.row, a.col, result_data);
}

fn :: vlinalg subtract(a :: Matrix, b :: Matrix) -> Matrix {
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

    return Matrix(a.row, a.col, result_data);
}

fn :: vlinalg rref(matrix :: Matrix) -> Matrix {
    m = [];
    
    through i :: 0..matrix.row-1 -> loop {
        new_row = [];
        through j :: 0..matrix.col-1 -> loop {
            new_row.push(matrix.data[i][j]);
        };
        m.push(new_row);    };
    
    lead = 0;
    rowCount = matrix.row;
    colCount = matrix.col;
    
    through r :: 0..rowCount-1 -> loop {
        if lead >= colCount {
            return Matrix(rowCount, colCount, m);
        }
        
        i = r;
        while m[i][lead] == 0 {
            i = i + 1;
            if i == rowCount {
                i = r;
                lead = lead + 1;
                if lead == colCount {
                    return Matrix(rowCount, colCount, m);
                }
            }
        }
        
        if i != r {
            temp = m[i];
            m[i] = m[r];
            m[r] = temp;
        };
        
        val = m[r][lead];
        if val != 0 {
            through j :: 0..colCount-1 -> loop {
                m[r][j] = m[r][j] / val;
            };
        };
        
        through i :: 0..rowCount-1 -> loop {
            if i != r {
                val = m[i][lead];
                through j :: 0..colCount-1 -> loop {
                    m[i][j] = m[i][j] - val * m[r][j];
                };
            };
        };
        
        lead = lead + 1;
    };
    
    return Matrix(rowCount, colCount, m);
}

deploy vlinalg;