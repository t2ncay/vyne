interface Element {
    symbol :: String,
    atomic_mass :: Float64,
    valency :: Int64
}

interface Something {
    e :: Element
}

hydrogen = Element("H", 1.008, 1); 
tester = Something(hydrogen);

out(tester);
out(type(hydrogen));