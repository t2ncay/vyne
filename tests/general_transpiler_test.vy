# Vyne transpiler smoke test
# Only exercises what codegen.cpp currently supports.

ruleset { dynamic_casting, warnings };

# --- Basic math ---
out("=== math ===");
x :: Int64 = 5;
y :: Int64 = 10;
z :: Int64 = x + y;
out(z);

# --- String concat ---
greeting = "hello, " + "world";
out(greeting);

# --- Boolean + comparison ---
isBig = z > 20;
out(isBig);

# --- If / else ---
if z > 20 {
    out("big");
} else {
    out("small");
}

# --- While loop ---
count :: Int64 = 0;
while count < 3 {
    count = count + 1;
}
out(count);

# --- For loop (simple, LOOP mode) ---
through i :: 0..5 loop {
    out(i);
};

# --- Functions ---
fn add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

fn square(n :: Int64) -> Int64 {
    return n * n;
}

out(add(3, 4));
out(square(7));

# --- Arrays + index ---
arr :: Array = [10, 20, 30, 40];
out(arr);
out(arr[1]);

# --- Ternary ---
msg = z > 10 ? "yes" : "no";
out(msg);

# --- Built-ins ---
out(type(42));
out(int64(3.99));
out(float64(7));
out(string(123));
out(sizeof(arr));

# --- End ---
out("done");