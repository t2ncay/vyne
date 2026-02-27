module vlinalg;

group Types :: vlinalg {
    interface Matrix {
        row_length    :: Int64,
        column_length :: Int64
    }

    x = 5;
};

deploy vlinalg;