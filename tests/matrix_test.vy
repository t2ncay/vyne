use extern "vlinalg.vy";

m1 :: vlinalg.Matrix = vlinalg.Matrix(2, 3, [
    [1, 2, 3],
    [4, 5, 6]
]);

m2 :: vlinalg.Matrix = vlinalg.Matrix(2, 3, [
    [1, 2, 3],
    [4, 5, 6]
]);

result = vlinalg.add(m1, m2);

out("Resulting Matrix:");
out(result);