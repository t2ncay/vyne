# float32_test.vy - first-class Float32 scalars, arrays, and promotion.
#
#   ./vynec --compile examples/test/float32_test.vy
#   ./examples/test/float32_test

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

fn close32(a :: Float32, b :: Float32) -> Bool {
    d :: Float32 := a - b;
    if d < 0.0f { d = 0.0f - d; }
    return d < 0.0001f;
}

out("=== Float32 ===");

a :: Float32 := 1.5f;
b :: Float32 := 2.25f;
s :: Float32 := a + b;

check("literal-type", type(a) == "Float32");
check("f32-add-type", type(s) == "Float32");
check("f32-add-value", close32(s, 3.75f));

f :: Float64 := 2.0;
mixed :: Float64 := a + f;
check("mixed-promotes-f64", type(mixed) == "Float64");
check("mixed-value", mixed == 3.5);

c :: Float32 := float32(9);
check("float32-cast", type(c) == "Float32");
check("float32-cast-value", c == 9.0f);

arr :: Array<Float32> := [1.0f, 2.0f, 3.0f];
check("array-elem-type", type(arr[0]) == "Float32");
check("array-value", arr[0] == 1.0f);

arr[1] = a + b;
check("array-assign", close32(arr[1], 3.75f));

arr.push(4.5f);
check("array-push-size", sizeof(arr) == 4);
check("array-push-value", close32(arr[sizeof(arr) - 1], 4.5f));

fn dot_f32(x :: Array<Float32>, y :: Array<Float32>, n :: Int64) -> Float32 {
    total :: Float32 := 0.0f;
    through i :: 0..n - 1 -> loop {
        total = total + x[i] * y[i];
    };
    return total;
}

d :: Float32 := dot_f32(arr, arr, sizeof(arr));
check("native-f32-array", type(d) == "Float32");
check("native-f32-value", close32(d, 44.3125f));

if failures[0] > 0 {
    out("float32: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("float32: all checks passed");
