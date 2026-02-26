module vcore;
module vmem;

interface Element {
    symbol :: String,
    atomic_mass :: Float64,
    valency :: Int64
}

hydrogen = Element("H", 1.008, 1); 

out(hydrogen.atomic_mass);
out(type(hydrogen));