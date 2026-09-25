#===============================================================================
# Generic syntax smoke test
# Copyright (c) 2026 Tuncay Gafarli — MIT License
#===============================================================================

ruleset { dynamic_casting };

interface Box<T> {
    value :: T;
}

interface Pair<A, B> {
    first  :: A;
    second :: B;
}

interface Nested<T> {
    items :: Array<T>;
}

fn identity<T>(x :: T) -> T {
    return x;
}

fn wrap<T>(value :: T) -> Box<T> {
    return Box<T>(value);
}

fn main() {
    out("=== Generic interface construction ===");

    b1 :: Box<Int64>  = Box<Int64>(42);
    b2 :: Box<String> = Box<String>("hello");
    out(b1.value);            # 42
    out(b2.value);            # hello

    out("");
    out("=== Multi-param generics ===");

    p :: Pair<Int64, String> = Pair<Int64, String>(7, "seven");
    out(p.first);             # 7
    out(p.second);            # seven

    out("");
    out("=== Nested generic annotations ===");

    n :: Nested<Int64> = Nested<Int64>([1, 2, 3]);
    out(n.items);             # [1, 2, 3]

    out("");
    out("=== Generic functions ===");

    out(identity<Int64>(99));             # 99
    out(identity<String>("world"));       # world
    out(identity<Float64>(3.14));         # 3.14

    out("");
    out("=== Plain types still work ===");

    x :: Int64   = 5;
    y :: Float64 = 2.5;
    s :: String  = "ok";
    a :: Array   = [10, 20, 30];

    out(x);       # 5
    out(y);       # 2.5
    out(s);       # ok
    out(a);       # [10, 20, 30]

    out("");
    out("=== DONE ===");
}

main();