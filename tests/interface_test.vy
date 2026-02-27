interface Element {
    atomic_mass :: Int64,
    name :: String
}

hydrogen :: Element = Element(1.008, "H");
out(hydrogen);