group Master {
    interface Element {
        atomic_mass :: Int64,
        name :: String
    }
};

hydrogen :: Master.Element = Master.Element();
hydrogen.name = "E";
out(hydrogen.atomic_mass);
out(hydrogen);