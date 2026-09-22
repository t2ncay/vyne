ruleset { warnings, verbose, dynamic_casting };

# Triggers: unused variable warning
unused_lonely :: Int64 = 42;

# Triggers: implicit typing warning
implicitOne = "hello";

# Triggers: shadow warning
x :: Int64 = 10;
fn shadowed() -> Int64 {
    x :: Int64 = 20;
    return x;
}
out(shadowed());

# Triggers: reassignment warning (if your parser flags this)
y :: Int64 = 1;
y = 2;
out(y);

out("done");