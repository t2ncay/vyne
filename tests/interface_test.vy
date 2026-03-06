module custom;

group Master :: custom {
    interface Element {
        atomic_mass :: Int64,
        name :: String,

        getName() {
            if self.name == "" {
                return "Empty";
            } else {
                return self.name;
            }
        }
    }
};

interface Node {
    kind :: Int64,
    data :: Int64, 
    left :: Node&,
    right :: Node& 
}

hydrogen :: custom.Master.Element = custom.Master.Element();
out(hydrogen.atomic_mass);
out(hydrogen);
out(hydrogen.getName());

node :: Node = Node();