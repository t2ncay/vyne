use extern "vmatrix.vy";

matrix_1 :: Array = [
    [1,2,3],
    [4,5,6]
];

matrix_2 :: Array = [
    [1,2,3],
    [4,5,6]
];

out(vmatrix.matrix_add(matrix_1, matrix_2));