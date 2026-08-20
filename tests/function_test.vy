ruleset { dynamic_casting };

num = 1;
arr = [1, 2, 3];

fn modify(a :: Array&) -> Array {
    a.push(99);
}

if (num in arr) {
    out("dih");
}

modify(arr);
out(arr);