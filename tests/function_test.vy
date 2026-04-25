ruleset { warnings };

arr = [1, 2, 3];

fn modify(a :: Array&) -> Array {
    a.push(99);
}

modify(arr);
out(arr);