ruleset {
    dynamic_casting,
    verbose
};

fn slice_demo(a :: Array, lo :: Int64, hi :: Int64) -> Array {
    return a[lo:hi];
}

fn slice_from(a :: Array, lo :: Int64) -> Array {
    return a[lo:];
}

fn slice_upto(a :: Array, hi :: Int64) -> Array {
    return a[:hi];
}

data = [10, 20, 30, 40, 50, 60];
out(slice_demo(data, 1, 3));    # -> [20, 30, 40]
out(slice_from(data, 3));       # -> [40, 50, 60]
out(slice_upto(data, 2));       # -> [10, 20, 30]

s = "hello world";
out(s[0:4]);                    # -> "hell"
out(s[6:]);                     # -> "world"
out(s[:5]);                     # -> "hello"