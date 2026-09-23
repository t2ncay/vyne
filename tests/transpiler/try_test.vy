ruleset { dynamic_casting };

out("=== try completes normally ===");

fn safe_div(a :: Int64, b :: Int64) -> Int64 {
    try {
        if b == 0 {
            throw "division by zero";
        }
        return a / b;
    } catch (e) {
        out("caught: " + e);
        return -1;
    }
}

out(safe_div(10, 2));
out(safe_div(10, 0));

out("=== finally runs on both paths ===");

fn with_finally(x :: Int64) -> Int64 {
    try {
        out("try");
        if x > 0 { throw "boom"; }
        return 1;
    } catch (e) {
        out("catch: " + e);
        return 2;
    } finally {
        out("finally");
    }
}

out(with_finally(0));
out(with_finally(1));

out("=== nested try ===");

fn nested() -> Int64 {
    try {
        try {
            throw "inner";
        } catch (e) {
            out("inner caught: " + e);
            throw "outer";
        } finally {
            out("inner finally");
        }
    } catch (e) {
        out("outer caught: " + e);
        return 99;
    } finally {
        out("outer finally");
    }
}

out(nested());

out("=== return inside try runs finally ===");

fn ret_in_try() -> Int64 {
    try {
        return 42;
    } finally {
        out("cleanup after return");
    }
}

out(ret_in_try());

out("=== uncaught propagates ===");

fn broken() -> Int64 {
    throw "unhandled";
}

try {
    broken();
} catch (e) {
    out("outer: " + e);
}

out("done");