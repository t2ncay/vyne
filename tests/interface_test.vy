interface Element {
    symbol      :: String,
    atomic_mass :: Float64,
    valency     :: Int64
}

interface Chemical {
    symbol      :: String,
    atomic_mass :: Float64,
    valency     :: Int64,
}

fn inspect(item :: Element) {
    out("--- Element Report ---");
    out("Symbol: " + item.symbol);
    out("Mass:   " + item.atomic_mass);
    out("Valency: " + item.valency);
}

hydrogen = Chemical(
    "H", 
    1.008, 
    1
);

interface Container {
    inner :: Element
}

box = Container(hydrogen);

out("Direct access: " + hydrogen.symbol);
out("Nested access: " + string(box.inner.valency));
out(hydrogen.fields());