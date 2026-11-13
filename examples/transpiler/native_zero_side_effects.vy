# A discarded zero-argument native call must still execute.
use native vmath;
ruleset { dynamic_casting };

vmath.seed(1234);
first :: Float64 := vmath.random_float();
expected :: Float64 := vmath.random_float();
vmath.seed(1234);
vmath.random_float();
actual :: Float64 := vmath.random_float();
if (actual != expected) {
    throw "discarded nativeZero call did not advance random state";
}
out("nativeZero side effects: PASS");
