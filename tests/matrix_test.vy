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
vector2 :: vlinalg.Types.Vector = vlinalg.Types.Vector(5,6);

out(vlinalg.add(m1, m2));
out(m1.insert_row([7,8,9]));