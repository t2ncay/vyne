module custom;

group Master :: custom {
    interface Element {
        atomic_mass :: Int64,
        name :: String
    }
};

hydrogen :: custom.Master.Element = custom.Master.Element();
hydrogen.name = "E";
out(hydrogen.atomic_mass);
out(hydrogen);
out(hydrogen.fields());