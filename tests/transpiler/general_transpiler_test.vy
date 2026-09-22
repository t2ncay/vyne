ruleset { dynamic_casting, warnings };

# ============================================================
# 1. Scope mangling: same-name locals in different functions
# ============================================================
out("=== scope mangling ===");

fn f() -> Int64 {
    x :: Int64 = 100;
    return x;
}
fn g() -> Int64 {
    x :: Int64 = 200;
    return x;
}
out(f());
out(g());

# shadow a global
counter :: Int64 = 0;
fn bump() -> Int64 {
    counter = counter + 1;
    return counter;
}
out(bump());
out(bump());
out(counter);

# ============================================================
# 2. Enums
# ============================================================
out("=== enums ===");

enum Color {
    Red,
    Green = 10,
    Blue
};

out(Color.Red);
out(Color.Green);
out(Color.Blue);

# ============================================================
# 3. String methods
# ============================================================
out("=== string methods ===");

s = "  Hello, World!  ";
out(s.trim());
out(s.trim().uppercase());
out(s.trim().lowercase());
out(s.trim().substr(0, 5));
out(s.trim().find("World"));
out(s.trim().replace("World", "Vyne"));

# ============================================================
# 4. Deep copy: mutate array inside function, caller unaffected
# ============================================================
out("=== deep copy ===");

fn mutate(arr :: Array) {
    arr[0] = 999;
    return null;
}

data :: Array = [1, 2, 3];
out(data);
mutate(data);
out(data);   # should STILL be [1, 2, 3]

# ============================================================
# Done
# ============================================================
out("done");