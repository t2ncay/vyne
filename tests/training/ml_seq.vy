# ml_seq.vy — RNA sequence classifier.
#
# Task: distinguish codon-structured RNA from uniform-random RNA
# using 64-dimensional codon-usage vectors (3-mer composition).
#
# Structured sequences are built by reverse-translating random
# 10-residue proteins through vbio's codon table — so every 3-mer
# comes from a fixed 20-codon subset. Random sequences draw from
# all 64 codons uniformly. The classifier learns the subset.
#
# This is a scaled-down version of a real, decades-old technique:
# codon-usage / k-mer composition has been the workhorse feature
# for gene finding, species identification from metagenomes, and
# detecting horizontal gene transfer since the 1980s — and still
# shows up in modern pipelines as a cheap prior before deep models.
#
# Requires:
#   vbio/bio.vy       (the bio facade)
#   vlinalg.vy        (monolithic or split facade — same API)
#   vcolors.vy

ruleset { dynamic_casting };

use lib "vbio/bio.vy";
use lib "vlinalg/vlinalg.vy";
use lib "vcolors.vy";

module vmath;
module vmem;

# ======================================================================
# CONFIG
# ======================================================================
N_PER_CLASS = 120;      # sequences per class
SEQ_LEN     = 30;       # nucleotides per sequence (10 codons)
EPOCHS      = 50;
LR          = 0.5;
HIDDEN1     = 16;
HIDDEN2     = 12;
PRINT_EVERY = 50;

N_SAMPLES = N_PER_CLASS * 2;

# ======================================================================
# 64-codon table in canonical order. RNA alphabet.
# ======================================================================
CODONS :: Array = [
    "AAA","AAC","AAG","AAU","ACA","ACC","ACG","ACU",
    "AGA","AGC","AGG","AGU","AUA","AUC","AUG","AUU",
    "CAA","CAC","CAG","CAU","CCA","CCC","CCG","CCU",
    "CGA","CGC","CGG","CGU","CUA","CUC","CUG","CUU",
    "GAA","GAC","GAG","GAU","GCA","GCC","GCG","GCU",
    "GGA","GGC","GGG","GGU","GUA","GUC","GUG","GUU",
    "UAA","UAC","UAG","UAU","UCA","UCC","UCG","UCU",
    "UGA","UGC","UGG","UGU","UUA","UUC","UUG","UUU"
];

AA_ALPHABET :: Array = ["A","R","N","D","C","Q","E","G","H","I",
                        "L","K","M","F","P","S","T","W","Y","V"];

# ======================================================================
# HELPERS
# ======================================================================
fn pad_left(s :: String, width :: Int64) -> String {
    n      = int64(s.size());
    result = "";
    through i :: 0..width-n-1 -> loop { result = result + " "; };
    return result + s;
}

fn pct(x :: Float64) -> String {
    scaled = int64(x * 1000.0);
    ip     = scaled / 10;
    fp     = scaled % 10;
    return string(ip) + "." + string(fp) + "%";
}

fn bar(fraction :: Float64, width :: Int64) -> String {
    filled = int64(fraction * float64(width));
    if filled < 0     { filled = 0; }
    if filled > width { filled = width; }
    s = "[";
    through i :: 0..width-1 -> loop {
        if i < filled { s = s + "#"; } else { s = s + "."; }
    };
    return s + "]";
}

# ======================================================================
# SEQUENCE GENERATION
# ======================================================================
fn trim(s :: String, n :: Int64) -> String {
    # Explicit char-by-char trim — avoids relying on slice
    # semantics that differ between interp and transpiler.
    output = "";
    through i :: 0..n-1 -> loop {
        output = output + s[i];
    };
    return output;
}

fn make_coding() -> String {
    # 10 random amino acids -> reverse_translate -> 33 nt RNA.
    # Trim to SEQ_LEN so both classes have identical length.
    prot = "";
    through i :: 0..9 -> loop {
        idx = vmath.random(0, 19);
        prot = prot + AA_ALPHABET[idx];
    };
    full = bio.reverse_translate(prot);   # 33 nt (30 + UAA stop)
    return trim(full, SEQ_LEN);
}

fn make_random() -> String {
    bases :: Array = ["A","U","G","C"];
    s = "";
    through i :: 0..SEQ_LEN-1 -> loop {
        idx = vmath.random(0, 3);
        s = s + bases[idx];
    };
    return s;
}

# ======================================================================
# FEATURE EXTRACTION — 64-dim codon-usage vector
# ======================================================================
fn codon_features(seq :: String) -> Array {
    counts = bio.codon_usage(seq);
    n_codons = seq.size() / 3;
    total = float64(n_codons);
    if total < 1.0 { total = 1.0; }

    features :: Array = [];
    through i :: 0..63 -> loop {
        c = CODONS[i];
        v = 0.0;
        if counts.has(c) {
            v = float64(counts[c]) / total;
        }
        features.push(v);
    };
    return features;
}

# ======================================================================
# NETWORK
# ======================================================================
fn forward(X_in, W1, b1, W2, b2, W3, b3) -> Array {
    A1 = vlinalg.apply_tanh(vlinalg.add_bias(vlinalg.multiply(X_in, W1), b1));
    A2 = vlinalg.apply_tanh(vlinalg.add_bias(vlinalg.multiply(A1,   W2), b2));
    A3 = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(A2, W3), b3));
    return [A1, A2, A3];
}

fn accuracy(A3, Y, n :: Int64) -> Float64 {
    correct = 0;
    through i :: 0..n-1 -> loop {
        p  = A3.data[i];
        a  = Y.data[i];
        pi = 0;
        ai = 0;
        if p > 0.5 { pi = 1; }
        if a > 0.5 { ai = 1; }
        if pi == ai { correct = correct + 1; }
    };
    return float64(correct) / float64(n);
}

# ======================================================================
# HEADER
# ======================================================================
out("");
out(vcolors.bold("=== RNA Sequence Classifier ==="));
out("");
out("  task           codon-structured vs uniform-random");
out("  features       64-dim codon usage (3-mer composition)");
out("  architecture   64 -> " + string(HIDDEN1) + " -> " + string(HIDDEN2) + " -> 1");
out("  samples        " + string(N_SAMPLES) + " (" + string(N_PER_CLASS) + " per class)");
out("  seq length     " + string(SEQ_LEN) + " nt");
out("  learning rate  " + string(LR));
out("  epochs         " + string(EPOCHS));
out("");

# ======================================================================
# DATA
# ======================================================================
out("Generating sequences...");

X_flat :: Array = [];
Y_data :: Array = [];
seqs   :: Array = [];

# Class 1 — codon-structured RNA
through i :: 0..N_PER_CLASS-1 -> loop {
    s = make_coding();
    seqs.push(s);
    feats = codon_features(s);
    through j :: 0..63 -> loop {
        X_flat.push(feats[j]);
    };
    Y_data.push(1.0);
};

# Class 0 — uniform-random RNA
through i :: 0..N_PER_CLASS-1 -> loop {
    s = make_random();
    seqs.push(s);
    feats = codon_features(s);
    through j :: 0..63 -> loop {
        X_flat.push(feats[j]);
    };
    Y_data.push(0.0);
};

X = vlinalg.Types.Matrix(N_SAMPLES, 64, X_flat);
Y = vlinalg.Types.Matrix(N_SAMPLES, 1,  Y_data);

out("  class 1 (structured) " + string(N_PER_CLASS));
out("  class 0 (random)     " + string(N_PER_CLASS));
out("");

# ======================================================================
# WEIGHTS
# ======================================================================
W1 = vlinalg.xavier_init(64,      HIDDEN1);
W2 = vlinalg.xavier_init(HIDDEN1, HIDDEN2);
W3 = vlinalg.xavier_init(HIDDEN2, 1);

b1 :: Array = [];
through j :: 0..HIDDEN1-1 -> loop { b1.push(0.0); };

b2 :: Array = [];
through j :: 0..HIDDEN2-1 -> loop { b2.push(0.0); };

b3 :: Array = [0.0];

# ======================================================================
# TRAINING
# ======================================================================
scale = LR / float64(N_SAMPLES);

h0    = forward(X, W1, b1, W2, b2, W3, b3);
loss0 = vlinalg.cross_entropy(h0[2], Y);

out(vcolors.bold("Training:"));
out("  initial loss  " + string(loss0));
out("");

lossN = loss0;
A3    = h0[2];

through epoch :: 1..EPOCHS -> loop {
    region training {
        scratch db2_buf :: Float64[12];
        scratch db1_buf :: Float64[16];

        h  = forward(X, W1, b1, W2, b2, W3, b3);
        A1 = h[0];
        A2 = h[1];
        A3 = h[2];

        # A3 is read *after* the loop (final_acc = accuracy(A3, Y, ...)).
        # A plain `region` rewinds the arena at its closing brace, which
        # would leave v_A3 pointing at freed scratch. commit() deep-clones
        # A3 into the commit arena so it survives the rewind.
        # Only the last iteration's A3 actually matters, so gate it.
        if epoch == EPOCHS {
            region.commit(A3);
        }

        # ---- backprop ----
        delta3 = vlinalg.subtract(A3, Y);
        dW3    = vlinalg.multiply(vlinalg.transpose(A2), delta3);

        db3 = 0.0;
        through r :: 0..N_SAMPLES-1 -> loop { db3 = db3 + delta3.data[r]; };

        delta2 = vlinalg.hadamard(
            vlinalg.multiply(delta3, vlinalg.transpose(W3)),
            vlinalg.tanh_prime(A2)
        );
        dW2 = vlinalg.multiply(vlinalg.transpose(A1), delta2);

        delta1 = vlinalg.hadamard(
            vlinalg.multiply(delta2, vlinalg.transpose(W2)),
            vlinalg.tanh_prime(A1)
        );
        dW1 = vlinalg.multiply(vlinalg.transpose(X), delta1);

        # --- db2 accumulation ---
        through c :: 0..HIDDEN2-1 -> loop {
            db2_buf[c] = 0.0;
            through r :: 0..N_SAMPLES-1 -> loop {
                db2_buf[c] = db2_buf[c] + delta2.data[r * HIDDEN2 + c];
            };
        };

        # --- db1 accumulation ---
        through c :: 0..HIDDEN1-1 -> loop {
            db1_buf[c] = 0.0;
            through r :: 0..N_SAMPLES-1 -> loop {
                db1_buf[c] = db1_buf[c] + delta1.data[r * HIDDEN1 + c];
            };
        };

        # --- SGD bias update, reading from scratch ---
        through c :: 0..HIDDEN1-1 -> loop {
            b1[c] = b1[c] - scale * db1_buf[c];
        };
        through c :: 0..HIDDEN2-1 -> loop {
            b2[c] = b2[c] - scale * db2_buf[c];
        };
        b3[0] = b3[0] - scale * db3;

        # ---- progress ----
        if epoch % PRINT_EVERY == 0 {
            lossN = vlinalg.cross_entropy(A3, Y);
            acc   = accuracy(A3, Y, N_SAMPLES);
            out("  " + pad_left(string(epoch), 5) + "/" + string(EPOCHS)
                + "  loss " + string(lossN)
                + "  acc  " + pct(acc)
                + "  " + bar(acc, 18));
        }
    };
};

# ======================================================================
# FINAL REPORT
# ======================================================================
final_acc = accuracy(A3, Y, N_SAMPLES);

out("");
out(vcolors.bold("Training complete."));
out("  initial loss   " + string(loss0));
out("  final loss     " + string(lossN));
out("  final accuracy " + pct(final_acc));
out("");

# ======================================================================
# SAMPLE PREDICTIONS
# ======================================================================
out(vcolors.bold("Sample predictions:"));
out("");

through i :: 0..5 -> loop {
    idx = i;
    s   = seqs[idx];
    p   = A3.data[idx];
    tag = "  random";
    if p > 0.5 { tag = "  struct"; }
    out(tag + "  " + s + "   p(struct) = " + string(p));
};

out("");

through i :: 0..5 -> loop {
    idx = N_PER_CLASS + i;
    s   = seqs[idx];
    p   = A3.data[idx];
    tag = "  random";
    if p > 0.5 { tag = "  struct"; }
    out(tag + "  " + s + "   p(struct) = " + string(p));
};

out("");
out(vcolors.success("Done."));