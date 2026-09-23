ruleset {
    verbose,
    dynamic_casting
};

use "math_utils.vy";
use "shapes.vy";

fn main() {
    out(add(3, 4));
    out(square(5));

    p :: Point = Point(10, 20);
    out(p.x);
    out(p.y);

    out(distance_sq(3, 4));
}

main();