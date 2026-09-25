# bio/Sequence.vy — sequence primitives.
#
# All functions operate on plain Strings. The DNA/RNA wrappers exist for
# callers who want type safety and `describe()`, but they carry only the
# raw string — so these helpers work on either.

use "Types.vy";

ruleset {
    dynamic_casting
};

module bio;

# --- Nucleotide-level -----------------------------------------------------

fn :: bio complement_base(b :: String) -> String {
    if b == "A" { return "T"; }
    if b == "T" { return "A"; }
    if b == "G" { return "C"; }
    if b == "C" { return "G"; }
    if b == "U" { return "A"; }
    return "N";
}

fn :: bio complement(seq :: String) -> String {
    n :: Int64 = seq.size();
    output :: String = "";
    through i :: 0..n-1 -> loop {
        output = output + bio.complement_base(seq[i]);
    };
    return output;
}

fn :: bio reverse_string(seq :: String) -> String {
    n :: Int64 = seq.size();
    output :: String = "";
    through i :: 0..n-1 -> loop {
        output = output + seq[n - 1 - i];
    };
    return output;
}

fn :: bio reverse_complement(seq :: String) -> String {
    return bio.reverse_string(bio.complement(seq));
}

# --- DNA / RNA conveniences -----------------------------------------------

fn :: bio rc(d :: bio.Types.DNA) -> bio.Types.DNA {
    return bio.Types.DNA(bio.reverse_complement(d.seq), d.name + "_rc");
}

fn :: bio transcribe(d :: bio.Types.DNA) -> bio.Types.RNA {
    n :: Int64 = d.seq.size();
    output :: String = "";
    through i :: 0..n-1 -> loop {
        b :: String = d.seq[i];
        if b == "T" {
            output = output + "U";
        } else {
            output = output + b;
        }
    };
    return bio.Types.RNA(output, d.name + "_mrna");
}

# --- Composition ----------------------------------------------------------

fn :: bio count_base(seq :: String, b :: String) -> Int64 {
    n :: Int64 = seq.size();
    c :: Int64 = 0;
    through i :: 0..n-1 -> loop {
        if seq[i] == b {
            c = c + 1;
        }
    };
    return c;
}

fn :: bio gc_content(seq :: String) -> Float64 {
    n :: Int64 = seq.size();
    if n == 0 {
        return 0.0;
    }
    g :: Int64 = bio.count_base(seq, "G");
    c :: Int64 = bio.count_base(seq, "C");
    return float64(g + c) / float64(n);
}

# Wallace rule for short oligos (<14 nt):
#     Tm = 2 * (A + T) + 4 * (G + C)
# GC-based approximation for longer sequences:
#     Tm = 64.9 + 41 * (GC_count - 16.4) / N

fn :: bio melting_temperature(seq :: String) -> Float64 {
    n :: Int64 = seq.size();
    if n == 0 {
        return 0.0;
    }

    if n < 14 {
        a :: Int64 = bio.count_base(seq, "A");
        t :: Int64 = bio.count_base(seq, "T");
        g :: Int64 = bio.count_base(seq, "G");
        c :: Int64 = bio.count_base(seq, "C");
        return float64(2 * (a + t) + 4 * (g + c));
    }

    g :: Int64 = bio.count_base(seq, "G");
    c :: Int64 = bio.count_base(seq, "C");
    return 64.9 + 41.0 * (float64(g + c) - 16.4) / float64(n);
}

# --- Search ---------------------------------------------------------------

# First index of `substr` in `seq`, or -1.
fn :: bio find_first(seq :: String, substr :: String) -> Int64 {
    n :: Int64 = seq.size();
    m :: Int64 = substr.size();
    if m == 0 { return 0; }
    if m > n { return -1; }
    through i :: 0..n-m -> loop {
        window :: String = "";
        through j :: 0..m-1 -> loop {
            window = window + seq[i + j];
        };
        if window == substr {
            return i;
        }
    };
    return -1;
}

# Hamming distance — requires equal-length strings.
fn :: bio hamming_distance(a :: String, b :: String) -> Int64 {
    n :: Int64 = a.size();
    if b.size() != n {
        return -1;
    }
    d :: Int64 = 0;
    through i :: 0..n-1 -> loop {
        if a[i] != b[i] {
            d = d + 1;
        }
    };
    return d;
}