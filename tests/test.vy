# Test 1: Basic reference parameter
ruleset { dynamic_casting : on };
out("=== TEST 1: Basic reference parameter ===");

sa = null;

fn modify_by_value(x) {
    x = 100;
    out("Inside modify_by_value: x = " + string(x));
    return x;
}

fn modify_by_reference(x :: Int64&) {
    x = 200;
    out("Inside modify_by_reference: x = " + string(x));
    return x;
}

a = 5;
out("Before modify_by_value: a = " + string(a));
result1 = modify_by_value(a);
out("After modify_by_value: a = " + string(a));
out("Result: " + string(result1));
out("");

b = 5;
out("Before modify_by_reference: b = " + string(b));
result2 = modify_by_reference(b);
out("After modify_by_reference: b = " + string(b));
out("Result: " + string(result2));
out("");

# Test 2: Multiple parameters with mixed types
out("=== TEST 2: Mixed parameters ===");

fn mixed_params(val, ref :: Int64&, another_val) {
    val = val * 10;
    ref = ref * 10;
    another_val = another_val * 10;
    out("Inside mixed_params: val=" + string(val) + ", ref=" + string(ref) + ", another=" + string(another_val));
    return ref;
}

c = 3;
d = 4;
e = 5;
out("Before: c=" + string(c) + ", d=" + string(d) + ", e=" + string(e));
result3 = mixed_params(c, d, e);
out("After: c=" + string(c) + ", d=" + string(d) + ", e=" + string(e));
out("Result: " + string(result3));
out("");

# Test 3: Reference to struct
out("=== TEST 3: Struct reference ===");

interface Point {
    x :: Int64,
    y :: Int64
}

fn move_point(p :: Int64&, dx, dy) {
    p.x = p.x + dx;
    p.y = p.y + dy;
    out("Inside move_point: (" + string(p.x) + ", " + string(p.y) + ")");
}

pt :: Point = Point(10, 20);
out("Before move_point: (" + string(pt.x) + ", " + string(pt.y) + ")");
move_point(pt, 5, 5);
out("After move_point: (" + string(pt.x) + ", " + string(pt.y) + ")");
out("");

# Test 4: Array reference
out("=== TEST 4: Array reference ===");

fn modify_array(arr :: Array&, index, value) {
    arr[index] = value;
    out("Inside modify_array: arr[" + string(index) + "] = " + string(value));
}

nums = [1, 2, 3, 4, 5];
out("Before: " + string(nums));
modify_array(nums, 2, 99);
out("After: " + string(nums));