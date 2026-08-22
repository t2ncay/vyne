ruleset { dynamic_casting, warnings };

fn add(a, b) -> Int64 {
    return a + b;
}

fn multiply(a, b) -> Int64 {
    return a * b;
}

fn square(x) -> Int64 {
    return x * x;
}

fn is_even(x) -> Bool {
    return x % 2 == 0;
}

fn uppercase(s) -> String {
    return s;
}

value = 5 |> add(3);
out("5 |> add(3) = ", value);

result = 5 |> add(3) |> multiply(2);
out("5 |> add(3) |> multiply(2) = ", result);

squared = 4 |> square();
out("4 |> square() = ", squared);

text = "hello" |> uppercase();
out("'hello' |> uppercase() = ", text);

complex = 10 
    |> add(5)
    |> multiply(2)
    |> square();
out("Complex pipeline result: ", complex);