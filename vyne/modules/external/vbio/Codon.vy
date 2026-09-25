# bio/Codon.vy — the genetic code.
#
# Uses mRNA codons (U instead of T). If your source is DNA, transcribe it
# first (see bio/Sequence.vy). The codon table is a plain Map at module
# scope, so it's built once at program start and shared.

use "Types.vy";

ruleset {
    dynamic_casting
};

module bio;

# --- Codon table ----------------------------------------------------------
# mRNA codon (3-char String) -> 1-letter amino acid code.
# '*' marks a stop codon. Grouped by amino acid for readability.

const bio_rna_codons :: Map = {
    # Phe
    "UUU": "F", "UUC": "F",
    # Leu
    "UUA": "L", "UUG": "L", "CUU": "L", "CUC": "L", "CUA": "L", "CUG": "L",
    # Ile
    "AUU": "I", "AUC": "I", "AUA": "I",
    # Met (start)
    "AUG": "M",
    # Val
    "GUU": "V", "GUC": "V", "GUA": "V", "GUG": "V",
    # Ser
    "UCU": "S", "UCC": "S", "UCA": "S", "UCG": "S", "AGU": "S", "AGC": "S",
    # Pro
    "CCU": "P", "CCC": "P", "CCA": "P", "CCG": "P",
    # Thr
    "ACU": "T", "ACC": "T", "ACA": "T", "ACG": "T",
    # Ala
    "GCU": "A", "GCC": "A", "GCA": "A", "GCG": "A",
    # Tyr
    "UAU": "Y", "UAC": "Y",
    # His
    "CAU": "H", "CAC": "H",
    # Gln
    "CAA": "Q", "CAG": "Q",
    # Asn
    "AAU": "N", "AAC": "N",
    # Lys
    "AAA": "K", "AAG": "K",
    # Asp
    "GAU": "D", "GAC": "D",
    # Glu
    "GAA": "E", "GAG": "E",
    # Cys
    "UGU": "C", "UGC": "C",
    # Trp
    "UGG": "W",
    # Arg
    "CGU": "R", "CGC": "R", "CGA": "R", "CGG": "R", "AGA": "R", "AGG": "R",
    # Gly
    "GGU": "G", "GGC": "G", "GGA": "G", "GGG": "G",
    # Stop
    "UAA": "*", "UAG": "*", "UGA": "*"
};

# Canonical codon per amino acid, for reverse_translate.
# Biased toward E. coli K-12 usage; change freely.

const bio_aa_to_codon :: Map = {
    "A": "GCG", "R": "CGU", "N": "AAC", "D": "GAU",
    "C": "UGC", "Q": "CAG", "E": "GAA", "G": "GGC",
    "H": "CAC", "I": "AUU", "L": "CUG", "K": "AAA",
    "M": "AUG", "F": "UUU", "P": "CCG", "S": "AGC",
    "T": "ACC", "W": "UGG", "Y": "UAC", "V": "GUG"
};

# --- Accessors ------------------------------------------------------------

fn :: bio codon_to_aa(codon :: String) -> String {
    if bio_rna_codons.has(codon) {
        return bio_rna_codons[codon];
    }
    return "X";
}

fn :: bio is_stop_codon(codon :: String) -> Bool {
    return bio.codon_to_aa(codon) == "*";
}

fn :: bio codon_for(aa :: String) -> String {
    if bio_aa_to_codon.has(aa) {
        return bio_aa_to_codon[aa];
    }
    return "NNN";
}

# --- Translation ----------------------------------------------------------
# Reads codons left-to-right; stops at the first stop codon. Anything after
# a stop is ignored (as a ribosome would). If the trailing codon is short,
# it is dropped (see the range bound n-3).

fn :: bio translate(mrna :: String) -> bio.Types.Protein {
    n :: Int64 = mrna.size();
    acc :: String = "";
    i :: Int64 = 0;
    while i <= n - 3 {
        codon :: String = mrna[i] + mrna[i + 1] + mrna[i + 2];
        aa    :: String = bio.codon_to_aa(codon);
        if aa == "*" {
            break;
        }
        acc = acc + aa;
        i = i + 3;
    }
    return bio.Types.Protein(acc, "translated");
}

fn :: bio translate_rna(r :: bio.Types.RNA) -> bio.Types.Protein {
    p :: bio.Types.Protein = bio.translate(r.seq);
    return bio.Types.Protein(p.seq, r.name + "_protein");
}

# --- Reverse translation --------------------------------------------------
# Produces one possible mRNA for the given protein. Stops are appended as
# UAA (a common choice). Not unique — many sequences encode the same
# protein.

fn :: bio reverse_translate(protein :: String) -> String {
    acc :: String = "";
    through i :: 0..protein.size()-1 -> loop {
        acc = acc + bio.codon_for(protein[i]);
    };
    return acc + "UAA";
}

# --- Composition helpers --------------------------------------------------

fn :: bio codon_usage(mrna :: String) -> Map {
    counts :: Map = {};
    n :: Int64 = mrna.size();
    i :: Int64 = 0;
    while i <= n - 3 {
        codon :: String = mrna[i] + mrna[i + 1] + mrna[i + 2];
        if counts.has(codon) {
            counts.set(codon, counts[codon] + 1);
        } else {
            counts.set(codon, 1);
        }
        i = i + 3;
    }
    return counts;
}