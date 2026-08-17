fn add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

hash :: Map = {
    "hello" : add(3,4)
};

out(hash.has("hello"));