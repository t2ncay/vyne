ruleset {  warnings, dynamic_casting };

module vcore;
module vglib;

A = 0.0;
B = 0.0;

while (false) {
    vglib.donut(A, B);
    A = A + 0.04;
    B = B + 0.02;
    vcore.sleep(10);
}
