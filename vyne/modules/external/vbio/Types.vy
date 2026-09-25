# bio/Types.vy — core interface types.
#
# Keep this file dependency-free: every other module in bio/ imports from
# here first. `seq` holds the raw character string; `name` is a label the
# caller controls (used only in describe() and by conveniences elsewhere).
#
# Alphabets assumed (but not enforced at runtime):
#   DNA     — A, C, G, T
#   RNA     — A, C, G, U
#   Protein — 20 standard 1-letter codes + '*' for stop

ruleset {
    dynamic_casting
};

module bio;

group Types :: bio {

    interface DNA {
        seq  :: String,
        name :: String,

        length() -> Int64 {
            return self.seq.size();
        }

        describe() -> String {
            return self.name + " (" + string(self.length()) + " bp)";
        }
    }

    interface RNA {
        seq  :: String,
        name :: String,

        length() -> Int64 {
            return self.seq.size();
        }

        describe() -> String {
            return self.name + " (" + string(self.length()) + " nt)";
        }
    }

    interface Protein {
        seq  :: String,
        name :: String,

        length() -> Int64 {
            return self.seq.size();
        }

        describe() -> String {
            return self.name + " (" + string(self.length()) + " aa)";
        }
    }
};

# --- Factories ------------------------------------------------------------
# Short constructors for callers who don't want to hand-write
# `bio.Types.DNA(s, "unnamed")`. Named variants go through the interface
# constructor directly.

fn :: bio make_dna(seq :: String) -> bio.Types.DNA {
    return bio.Types.DNA(seq, "unnamed");
}

fn :: bio make_rna(seq :: String) -> bio.Types.RNA {
    return bio.Types.RNA(seq, "unnamed");
}

fn :: bio make_protein(seq :: String) -> bio.Types.Protein {
    return bio.Types.Protein(seq, "unnamed");
}