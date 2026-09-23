use "numeric.vy";

interface Point {
    x :: Int64,
    y :: Int64,
}

fn distance_sq(x :: Int64, y :: Int64) -> Int64 {
    return mul(x, x) + mul(y, y);
}