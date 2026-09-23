ruleset { warnings, dynamic_casting };

out("=== basic named args ===");

fn greet(first :: String, last :: String) -> String {
    return "Hello, " + first + " " + last;
}

out(greet(first: "Alice", last: "Bob"));

out("=== out-of-order named args ===");

fn rect_area(width :: Int64, height :: Int64) -> Int64 {
    return width * height;
}

out(rect_area(height: 5, width: 10));   # 50

out("=== mixed-type named args ===");

fn describe(name :: String, age :: Int64, score :: Float64) -> String {
    return name + " is " + string(age) + " with score " + string(score);
}

out(describe(score: 95.5, name: "Bob", age: 30));

out("=== named args + recursion ===");

fn fact(n :: Int64) -> Int64 {
    if n <= 1 { return 1; }
    return n * fact(n: n - 1);
}

out(fact(n: 5));   # 120

out("=== named args return array ===");

fn pair_sum(a :: Int64, b :: Int64) -> Array {
    return [a + b, a - b];
}

result = pair_sum(b: 3, a: 10);
out(result);       # [13, 7]

out("done");