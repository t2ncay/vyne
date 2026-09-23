ruleset {
    warnings,
    dynamic_casting,
    verbose
};

group Master {
    interface Element {
        atomic_mass :: Int64,
        name :: String,

        getName() {
            return self.name;
        }
    }
};

hydrogen :: Master.Element = Master.Element(0.93842,"Hydrogen");
out(hydrogen.atomic_mass);
out(hydrogen);
out(hydrogen.getName());
out(type(hydrogen.atomic_mass))
