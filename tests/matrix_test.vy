use extern "vlinalg.vy";

m1 :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(2, 3, [
    [1, 2, 3],
    [4, 5, 6]
]);

m2 :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(2, 3, [
    [1, 2, 3],
    [4, 5, 6]
]);

vector :: vlinalg.Types.Vector = vlinalg.Types.Vector(2,3);
out(vector);
out(vector.slope());
out(vector.magnitude());