ruleset { warnings, dynamic_casting };

out("=== basic defer ===");

fn demo_basic() -> Int64 {
    defer out("cleanup 1");
    out("body 1");
    return 42;
}

out(demo_basic());
# Expected: body 1, cleanup 1, 42

out("=== LIFO order ===");

fn demo_lifo() -> String {
    defer out("first registered");
    defer out("second registered");
    defer out("third registered");
    out("body 2");
    return "done";
}

out(demo_lifo());
# Expected: body 2, third registered, second registered, first registered, done

out("=== early return runs defers ===");

fn demo_early(x :: Int64) -> Int64 {
    defer out("cleanup 3");
    if x > 0 {
        out("taking early return");
        return 100;
    }
    out("taking late return");
    return 200;
}

out(demo_early(1));
# Expected: taking early return, cleanup 3, 100

out(demo_early(-1));
# Expected: taking late return, cleanup 3, 200

out("=== defer with no return ===");

fn demo_noreturn() {
    defer out("cleanup 4");
    out("body 4");
}

demo_noreturn();
# Expected: body 4, cleanup 4

out("=== defer inside function with args ===");

fn scaled_double(x :: Int64) -> Int64 {
    defer out("scaled_double finished");
    result = x * 2;
    out(result);
    return result;
}

out(scaled_double(21));
# Expected: 42, scaled_double finished, 42

out("done");