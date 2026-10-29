# earthquake.vy — decision-tree classifier for earthquake prediction.
#
# Trains a CART tree on chronologically-ordered soil sensor readings.
# Each sample carries three features — mean soil displacement, delta
# from the previous reading, and cross-sensor variance — plus a binary
# target (1 = earthquake within 24h, 0 = no earthquake).
#
# Pipeline:
#   dataset     14 chronological sensor readings, 70/30 train/test
#   engine      recursive CART with Gini impurity
#   eval        confusion matrix + accuracy / precision / recall / F1
#   inference   single-sample prediction on an unseen spike

use external "vcolors.vy";

ruleset { dynamic_casting };

# ======================================================================
# CONFIG
# ======================================================================
MAX_DEPTH  :: Int64   := 3;
TRAIN_FRAC :: Float64 := 0.7;
N_FEATURES :: Int64   := 3;

# ======================================================================
# DATA STRUCTURES
# ======================================================================
interface EarthquakeSample {
    soilMean       :: Float64,
    soilDelta      :: Float64,
    sensorVariance :: Float64,
    target         :: Int64
}

# TreeNode has children so buildTree's recursion is meaningful.
# Boxed (non-C-eligible) because of the two self-referential struct
# fields — this is fine, the interpreter and codegen both handle
# boxed struct access.
interface TreeNode {
    featureIndex :: Int64,
    threshold    :: Float64,
    leafValue    :: Int64,
    isLeaf       :: Bool,
    left         :: TreeNode,
    right        :: TreeNode
}

# ======================================================================
# HELPERS
# ======================================================================

# Dynamic feature access. idx is a literal at every call site so the
# compiler statically resolves the field access.
fn getFeatureVal(sample :: EarthquakeSample, idx :: Int64) -> Float64 {
    if idx == 0 { return sample.soilMean; }
    if idx == 1 { return sample.soilDelta; }
    return sample.sensorVariance;
}

fn giniImpurity(samples :: Array) -> Float64 {
    total :: Float64 := float64(samples.size());
    if total == 0.0 { return 0.0; }

    posCount :: Int64 := 0;
    through s :: samples -> loop {
        if s.target == 1 { posCount = posCount + 1; }
    };

    p1 :: Float64 := float64(posCount) / total;
    p0 :: Float64 := 1.0 - p1;
    return 1.0 - (p1 * p1 + p0 * p0);
}

fn splitDataset(
    samples    :: Array,
    featureIdx :: Int64,
    threshold  :: Float64
) -> Array {
    leftSide  :: Array := [];
    rightSide :: Array := [];

    through s :: samples -> loop {
        val :: Float64 := getFeatureVal(s, featureIdx);
        if val <= threshold {
            leftSide.push(s);
        } else {
            rightSide.push(s);
        }
    };

    return [leftSide, rightSide];
}

fn majorityLabel(samples :: Array) -> Int64 {
    pos :: Int64 := 0;
    neg :: Int64 := 0;
    through s :: samples -> loop {
        if s.target == 1 { pos = pos + 1; } else { neg = neg + 1; }
    };
    if pos >= neg { return 1; }
    return 0;
}

fn isPure(samples :: Array) -> Bool {
    if samples.size() == 0 { return true; }
    firstTarget :: Int64 := samples[0].target;
    through s :: samples -> loop {
        if s.target != firstTarget { return false; }
    };
    return true;
}

# Formatting helpers, matching gene_finder.vy.
fn pad_left(s :: String, width :: Int64) -> String {
    n := int64(s.size());
    result := "";
    through i :: 0..width-n-1 -> loop { result = result + " "; };
    return result + s;
}

fn pct(x :: Float64) -> String {
    scaled := int64(x * 1000.0);
    ip := scaled / 10;
    fp := scaled % 10;
    return string(ip) + "." + string(fp) + "%";
}

# ======================================================================
# CART TRAINING
# ======================================================================
fn buildTree(samples :: Array, depth :: Int64, maxDepth :: Int64) -> TreeNode {
    # Leaf when empty, pure, or at max depth.
    if samples.size() == 0 || isPure(samples) || depth >= maxDepth {
        return TreeNode(-1, 0.0, majorityLabel(samples), true);
    }

    bestGini      :: Float64 := 999.0;
    bestFeature   :: Int64   := -1;
    bestThreshold :: Float64 := 0.0;
    leftSplit     :: Array   := [];
    rightSplit    :: Array   := [];

    through featureIdx :: 0..N_FEATURES-1 -> loop {
        through sample :: samples -> loop {
            thresh :: Float64 := getFeatureVal(sample, featureIdx);
            splits := splitDataset(samples, featureIdx, thresh);

            lSplit := splits[0];
            rSplit := splits[1];

            if lSplit.size() > 0 && rSplit.size() > 0 {
                wLeft  :: Float64 := float64(lSplit.size()) / float64(samples.size());
                wRight :: Float64 := float64(rSplit.size()) / float64(samples.size());
                gini   :: Float64 := (wLeft * giniImpurity(lSplit))
                                   + (wRight * giniImpurity(rSplit));

                if gini < bestGini {
                    bestGini      = gini;
                    bestFeature   = featureIdx;
                    bestThreshold = thresh;
                    leftSplit     = lSplit;
                    rightSplit    = rSplit;
                }
            }
        };
    };

    if bestFeature == -1 {
        return TreeNode(-1, 0.0, majorityLabel(samples), true);
    }

    leftNode  := buildTree(leftSplit, depth + 1, maxDepth);
    rightNode := buildTree(rightSplit, depth + 1, maxDepth);

    return TreeNode(
        bestFeature, bestThreshold, -1, false,
        leftNode, rightNode
    );
}

fn predictSingle(node :: TreeNode, sample :: EarthquakeSample) -> Int64 {
    if node.isLeaf { return node.leafValue; }

    val :: Float64 := getFeatureVal(sample, node.featureIndex);
    if val <= node.threshold {
        return predictSingle(node.left, sample);
    } else {
        return predictSingle(node.right, sample);
    }
}

# ======================================================================
# METRICS
# ======================================================================
fn evaluateMetrics(tp :: Int64, fp :: Int64, tn :: Int64, fnc :: Int64) -> Int64 {
    total :: Float64 := float64(tp + fp + tn + fnc);

    acc       :: Float64 := 0.0;
    precision :: Float64 := 0.0;
    recall    :: Float64 := 0.0;
    f1        :: Float64 := 0.0;

    if total > 0.0 {
        acc = float64(tp + tn) / total;
    }
    if (tp + fp) > 0 {
        precision = float64(tp) / float64(tp + fp);
    }
    if (tp + fnc) > 0 {
        recall = float64(tp) / float64(tp + fnc);
    }
    if (precision + recall) > 0.0 {
        f1 = 2.0 * (precision * recall) / (precision + recall);
    }

    out(vcolors.bold("  Confusion matrix"));
    out("    TP " + pad_left(string(tp), 3) + "   FP " + pad_left(string(fp), 3));
    out("    FN " + pad_left(string(fnc), 3) + "   TN " + pad_left(string(tn), 3));
    out("");
    out("  Accuracy   " + pct(acc));
    out("  Precision  " + string(precision));
    out("  Recall     " + string(recall));
    out("  F1-score   " + string(f1));
    return 0;
}

# ======================================================================
# HEADER
# ======================================================================
out("");
out(vcolors.bold("=== Earthquake Prediction (Decision Tree) ==="));
out("");
out("  task           binary classification, earthquake within 24h");
out("  features       soilMean, soilDelta, sensorVariance");
out("  split          chronological, " + pct(TRAIN_FRAC) + " / "
                                    + pct(1.0 - TRAIN_FRAC));
out("  max depth      " + string(MAX_DEPTH));
out("");

# ======================================================================
# DATASET
# ======================================================================
# Declared untyped Array and populated via push. A typed
# Array<EarthquakeSample> literal works for reads but breaks in
# `through x :: arr -> loop` — the for-loop lowering declares the
# iterator as a plain VyneValue, which is a C type error against the
# monomorphised container's vyne_EarthquakeSample elements. Untyped
# is the safe path today; this mirrors gene_finder's `seqs :: Array`.
out(vcolors.bold("1. Loading chronological sensor readings"));

dataset :: Array := [];
dataset.push(EarthquakeSample( 2.1,   0.1, 0.05, 0));
dataset.push(EarthquakeSample( 2.3,   0.2, 0.08, 0));
dataset.push(EarthquakeSample( 2.2,  -0.1, 0.04, 0));
dataset.push(EarthquakeSample( 3.5,   1.3, 0.45, 0));
dataset.push(EarthquakeSample( 5.8,   2.3, 1.12, 1));
dataset.push(EarthquakeSample(14.8,   9.0, 3.80, 1));
dataset.push(EarthquakeSample(16.1,   1.3, 4.10, 1));
dataset.push(EarthquakeSample(15.2,  -0.9, 3.50, 1));
dataset.push(EarthquakeSample( 8.4,  -6.8, 1.90, 0));
dataset.push(EarthquakeSample( 3.1,  -5.3, 0.30, 0));
dataset.push(EarthquakeSample( 2.8,  -0.3, 0.10, 0));
dataset.push(EarthquakeSample(12.4,   9.6, 2.95, 1));
dataset.push(EarthquakeSample(15.7,   3.3, 3.40, 1));
dataset.push(EarthquakeSample( 4.2, -11.5, 0.60, 0));

totalSamples :: Int64 := dataset.size();
splitIndex   :: Int64 := int64(float64(totalSamples) * TRAIN_FRAC);

trainData :: Array := [];
testData  :: Array := [];
through i :: 0..totalSamples-1 -> loop {
    if i < splitIndex {
        trainData.push(dataset[i]);
    } else {
        testData.push(dataset[i]);
    }
};

out("  total          " + pad_left(string(totalSamples), 4));
out("  train          " + pad_left(string(trainData.size()), 4)
                        + "  (first " + pct(TRAIN_FRAC) + ")");
out("  test           " + pad_left(string(testData.size()), 4)
                        + "  (last " + pct(1.0 - TRAIN_FRAC) + ")");
out("");

# ======================================================================
# TRAINING
# ======================================================================
out(vcolors.bold("2. Training decision tree"));
modelRoot :: TreeNode := buildTree(trainData, 0, MAX_DEPTH);
out("  trained        depth <= " + string(MAX_DEPTH));
out("");

# ======================================================================
# EVALUATION
# ======================================================================
out(vcolors.bold("3. Evaluation on held-out test set"));

tp  :: Int64 := 0;
fp  :: Int64 := 0;
tn  :: Int64 := 0;
fnc :: Int64 := 0;

through sample :: testData -> loop {
    pred :: Int64 := predictSingle(modelRoot, sample);
    if pred == 1 && sample.target == 1 { tp = tp + 1; }
    if pred == 1 && sample.target == 0 { fp = fp + 1; }
    if pred == 0 && sample.target == 0 { tn = tn + 1; }
    if pred == 0 && sample.target == 1 { fnc = fnc + 1; }
};

evaluateMetrics(tp, fp, tn, fnc);
out("");

# ======================================================================
# INFERENCE
# ======================================================================
out(vcolors.bold("4. Inference on unseen sample"));

unseen     :: EarthquakeSample := EarthquakeSample(14.5, 8.2, 3.10, 0);
prediction :: Int64 := predictSingle(modelRoot, unseen);

out("  soilMean       " + string(unseen.soilMean));
out("  soilDelta      " + string(unseen.soilDelta));
out("  sensorVariance " + string(unseen.sensorVariance));
out("");
if prediction == 1 {
    out(vcolors.bold("  >> high probability of an earthquake within 24h"));
} else {
    out(vcolors.bold("  >> low probability of an earthquake within 24h"));
}
out("");
