# All 146 raymath functions are available as vraylib.math_<snake_case>.
use "Types.vy";
use native vraylib;
ruleset { dynamic_casting };
module vraylib;

fn :: vraylib direction(from :: vraylib.Vector3, to :: vraylib.Vector3) -> vraylib.Vector3 {
    return vraylib.math_vector3_normalize(vraylib.math_vector3_subtract(to, from));
}
