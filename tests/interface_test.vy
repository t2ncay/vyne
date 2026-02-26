interface Element {
    symbol :: String,
    atomic_mass :: Float64,
    valency :: Int64
}

hydrogen = Element("H", 1.008, 1); 

out(hydrogen);
out(type(hydrogen));