module customs;

fn :: customs add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

out(customs.add(3,5));