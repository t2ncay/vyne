# gene_finder.vy — detect protein-coding DNA via the period-3 spectral signature.
#
# Protein-coding DNA has an elevated power spectrum component at frequency
# N/3: because codons are triplets, positions divisible by 3 have a base
# distribution that differs from the other two phases. Non-coding DNA is
# spectrally flat at that frequency. This script extracts the signature
# with vfft, assembles the feature vector with vlin, and trains a binary
# classifier with vml.
#
# Pipeline:
#   vbio — generate coding and non-coding windows
#   vfft — FFT each base-indicator sequence, extract period-3 power
#   vlin — assemble the feature matrix
#   vml  — train the classifier

use external "vbio/vbio.vy";
use external "vfft/vfft.vy";
use external "vml/vml.vy";
use external "vcolors.vy";

use native vmath;
use native vmem;

ruleset { dynamic_casting };

vmath.seed(42);

# ======================================================================
# CONFIG
# ======================================================================
N_PER_CLASS :: Int64 = 300;
WINDOW      :: Int64 = 256;            # power of two, so no FFT padding is needed
EPOCHS      :: Int64 = 500;
HIDDEN1     :: Int64 = 32;
HIDDEN2     :: Int64 = 16;
BATCH       :: Int64 = 32;
PRINT_EVERY :: Int64 = 50;

USE_CODON  :: Bool = false;

N_FEATURES = 5;              # base is always 4 period-3 + 1 GC
if USE_CODON { N_FEATURES = 69; }

N_SAMPLES  = N_PER_CLASS * 2;
N_BATCHES  = N_SAMPLES / BATCH;

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
fn trim(s :: String, n :: Int64) -> String {
    output = "";
    through i :: 0..n-1 -> loop { output = output + s[i]; };
    return output;
}

fn make_coding_window() -> String {
    # 86 amino acids → 86 codons → 258 nt, plus the stop codon that
    # reverse_translate appends. Trim to WINDOW (256).
    prot = "";
    through i :: 0..85 -> loop {
        idx = vmath.random(0, 19);
        prot = prot + AA_ALPHABET[idx];
    };
    full = vbio.reverse_translate(prot);
    return trim(full, WINDOW);
}

fn make_random_window() -> String {
    bases :: Array = ["A","U","G","C"];
    s = "";
    through i :: 0..WINDOW-1 -> loop {
        idx = vmath.random(0, 3);
        s = s + bases[idx];
    };
    return s;
}

# |X[k]|^2 summed over the period-3 bins, divided by total power.
#
# WINDOW = 64 → N/3 ≈ 21.33 → the peak lives at bins 21 and 22.
# For a uniform-random indicator, |X[k]|^2 is roughly flat and the
# ratio is ≈ 2/64 ≈ 0.031. For a codon-structured sequence, it is
# elevated because the indicator is periodic on average.
fn period3_ratio(window :: String, base :: String) -> Float64 {
    re :: Array<Float64> = [];
    im :: Array<Float64> = [];
    through i :: 0..WINDOW-1 -> loop {
        ch = window[i];
        if ch == base { re.push(1.0); } else { re.push(0.0); }
        im.push(0.0);
    };

    vfft.forward(re, im);

    total :: Float64 = 0.0;
    through k :: 1..WINDOW-1 -> loop {
        total = total + re[k]*re[k] + im[k]*im[k];
    };

    p3 :: Float64 = re[85]*re[85] + im[85]*im[85]
                  + re[86]*re[86] + im[86]*im[86];

    if total < 0.000000000001 { return 0.0; }
    return p3 / total;
}

fn window_features(window :: String) -> Array<Float64> {
    features :: Array<Float64> = [];

    # Spectral signature — one feature per base (4 dims).
    features.push(period3_ratio(window, "A"));
    features.push(period3_ratio(window, "C"));
    features.push(period3_ratio(window, "G"));
    features.push(period3_ratio(window, "U"));

    # GC content (1 dim).
    features.push(vbio.gc_content(window));

    # Codon usage (64 dims) — same as ml_seq.vy.
    if USE_CODON {
        counts = vbio.codon_usage(window);
        n_codons = window.size() / 3;
        total = float64(n_codons);
        if total < 1.0 { total = 1.0; }
        through i :: 0..63 -> loop {
            c = CODONS[i];
            v = 0.0;
            if counts.has(c) { v = float64(counts[c]) / total; }
            features.push(v);
        };
    }

    return features;
}

fn pad_left(s :: String, width :: Int64) -> String {
    n = int64(s.size());
    result = "";
    through i :: 0..width-n-1 -> loop { result = result + " "; };
    return result + s;
}

fn pct(x :: Float64) -> String {
    scaled = int64(x * 1000.0);
    ip = scaled / 10;
    fp = scaled % 10;
    return string(ip) + "." + string(fp) + "%";
}

# ======================================================================
# HEADER
# ======================================================================
out("");
out(vcolors.bold("=== Gene Finder (period-3 spectral classifier) ==="));
out("");
out("  task           coding vs non-coding, " + string(WINDOW) + "-nt windows");
if USE_CODON {
    out("  features       69-dim (4 period-3 + 1 GC + 64 codon)");
} else {
    out("  features       5-dim (4 period-3 + 1 GC)   [ablation]");
}
out("  samples        " + string(N_SAMPLES) + " (" + string(N_PER_CLASS) + " per class)");
out("  epochs         " + string(EPOCHS));
out("");

# ======================================================================
# FFT PLAN WARMUP
# ======================================================================
# Build the plan below every region checkpoint so no rewind can free it.
# See vfft/README.md — this is the one rule that must not be broken.
out("Warming FFT plan (N=" + string(WINDOW) + ")...");
dummy_re :: Array<Float64> = [];
dummy_im :: Array<Float64> = [];
through i :: 0..WINDOW-1 -> loop {
    dummy_re.push(0.0);
    dummy_im.push(0.0);
};
vfft.forward(dummy_re, dummy_im);

# ======================================================================
# DATA
# ======================================================================
out("Generating windows...");

X_flat :: Array<Float64> = [];
Y_data :: Array<Float64> = [];
seqs   :: Array = [];

through i :: 0..N_PER_CLASS-1 -> loop {
    s = make_coding_window();
    seqs.push(s);
    feats = window_features(s);
    through j :: 0..N_FEATURES-1 -> loop { X_flat.push(feats[j]); };
    Y_data.push(1.0);
};

through i :: 0..N_PER_CLASS-1 -> loop {
    s = make_random_window();
    seqs.push(s);
    feats = window_features(s);
    through j :: 0..N_FEATURES-1 -> loop { X_flat.push(feats[j]); };
    Y_data.push(0.0);
};

# Sanity diagnostic: is the period-3 [A] feature actually different?
# If the two numbers are close, the feature has no signal and training
# will not converge on anything other than the codon features.
meanA_coding :: Float64 = 0.0;
meanA_random :: Float64 = 0.0;
through i :: 0..N_PER_CLASS-1 -> loop {
    meanA_coding = meanA_coding + X_flat[i * N_FEATURES];
};
through i :: 0..N_PER_CLASS-1 -> loop {
    meanA_random = meanA_random + X_flat[(N_PER_CLASS + i) * N_FEATURES];
};
meanA_coding = meanA_coding / float64(N_PER_CLASS);
meanA_random = meanA_random / float64(N_PER_CLASS);
out("  mean period-3[A] coding:     " + string(meanA_coding));
out("  mean period-3[A] non-coding: " + string(meanA_random));
out("");

X :: vlin.Types.Matrix = vlin.Types.Matrix(N_SAMPLES, N_FEATURES, X_flat);
Y :: vlin.Types.Matrix = vlin.Types.Matrix(N_SAMPLES, 1, Y_data);

# ======================================================================
# MODEL
# ======================================================================
L1 = vml.dense(N_FEATURES, HIDDEN1, "tanh");
L2 = vml.dense(HIDDEN1, HIDDEN2, "tanh");
L3 = vml.dense(HIDDEN2, 1, "sigmoid");
model = vml.sequential([L1, L2, L3]);

W1 = model.layers[0].W;
W2 = model.layers[1].W;
W3 = model.layers[2].W;
b1 = model.layers[0].b;
b2 = model.layers[1].b;
b3 = model.layers[2].b;

opt_adam :: vml.Types.Adam = vml.adam(0.001);

state_W1 :: vml.Types.AdamState = vml.adam_state(N_FEATURES * HIDDEN1);
state_W2 :: vml.Types.AdamState = vml.adam_state(HIDDEN1 * HIDDEN2);
state_W3 :: vml.Types.AdamState = vml.adam_state(HIDDEN2 * 1);

indices :: Array = [];
through i :: 0..N_SAMPLES-1 -> loop { indices.push(i); };

batch_idx :: Array = [];
through i :: 0..BATCH-1 -> loop { batch_idx.push(0); };

# ======================================================================
# TRAINING
# ======================================================================
h0 = vml.forward_all_fused(model, X);
loss0 = vml.cross_entropy(h0[2], Y);

out(vcolors.bold("Training:"));
out("  initial loss   " + string(loss0));
out("");

t :: Int64 = 0;

through epoch :: 1..EPOCHS -> loop {
    vml.shuffle_indices(indices, N_SAMPLES);

    through b :: 0..N_BATCHES-1 -> loop {
        region step {
            scratch db2_buf :: Float64[16];
            scratch db1_buf :: Float64[32];

            through j :: 0..BATCH-1 -> loop {
                batch_idx[j] = indices[b * BATCH + j];
            };
            X_b = vml.gather_rows(X, batch_idx, BATCH);
            Y_b = vml.gather_rows(Y, batch_idx, BATCH);

            h = vml.forward_all_fused(model, X_b);
            A1 = h[0]; A2 = h[1]; A3 = h[2];

            delta3 = vlin.subtract(A3, Y_b);
            dW3 = vlin.multiply(vlin.transpose(A2), delta3);

            db3 = 0.0;
            through r :: 0..BATCH-1 -> loop { db3 = db3 + delta3.data[r]; };

            delta2 = vlin.hadamard(
                vlin.multiply(delta3, vlin.transpose(W3)),
                vlin.tanh_prime(A2));
            dW2 = vlin.multiply(vlin.transpose(A1), delta2);

            delta1 = vlin.hadamard(
                vlin.multiply(delta2, vlin.transpose(W2)),
                vlin.tanh_prime(A1));
            dW1 = vlin.multiply(vlin.transpose(X_b), delta1);

            t = t + 1;
            vml.adam_step(W1, dW1, state_W1, opt_adam, t);
            vml.adam_step(W2, dW2, state_W2, opt_adam, t);
            vml.adam_step(W3, dW3, state_W3, opt_adam, t);

            through c :: 0..HIDDEN2-1 -> loop {
                db2_buf[c] = 0.0;
                through r :: 0..BATCH-1 -> loop {
                    db2_buf[c] = db2_buf[c] + delta2.data[r * HIDDEN2 + c];
                };
            };
            through c :: 0..HIDDEN1-1 -> loop {
                db1_buf[c] = 0.0;
                through r :: 0..BATCH-1 -> loop {
                    db1_buf[c] = db1_buf[c] + delta1.data[r * HIDDEN1 + c];
                };
            };
            
            # using scratch values inside hidden batches are not recommended
            bscale :: Float64 = opt_adam.lr / float64(BATCH);
            through c :: 0..HIDDEN1-1 -> loop { b1[c] = b1[c] - bscale * db1_buf[c]; };
            through c :: 0..HIDDEN2-1 -> loop { b2[c] = b2[c] - bscale * db2_buf[c]; };
            b3[0] = b3[0] - bscale * db3;

            
        };
    };

    if epoch % PRINT_EVERY == 0 {
        h_eval = vml.forward_all_fused(model, X);
        A3_eval = h_eval[2];
        lossN = vml.cross_entropy(A3_eval, Y);
        acc   = vml.accuracy(A3_eval, Y, N_SAMPLES);
        out("  " + pad_left(string(epoch), 5) + "/" + string(EPOCHS)
            + "  loss " + string(lossN)
            + "  acc  " + pct(acc));
    }
};

# ======================================================================
# FINAL
# ======================================================================
h_final = vml.forward_all_fused(model, X);
A3_final = h_final[2];
final_acc = vml.accuracy(A3_final, Y, N_SAMPLES);

out("");
out(vcolors.bold("Done."));
out("  final accuracy " + pct(final_acc));
out("  checksum: " + string(final_acc));
out("");
