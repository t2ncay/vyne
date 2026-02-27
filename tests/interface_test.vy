group Master {
    interface Element {
        atomic_mass :: Int64,
        name :: String
    }
};

hydrogen :: Master.Element = Master.Element(1.008, "H");
hydrogen.name = "E";
out(hydrogen);