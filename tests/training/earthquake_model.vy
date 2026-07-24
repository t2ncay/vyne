ruleset { warnings, dynamic_casting };

module vcore;

#===============================================================================
# 1. DATA STRUCTURES & BLUEPRINTS (INTERFACES)
#===============================================================================

# Expanded sample schema using independent non-collinear features
interface EarthquakeSample {
    soilMean       :: Float64,
    soilDelta      :: Float64,
    sensorVariance :: Float64,
    target         :: Int64
}

# CART Tree Node blueprint
interface TreeNode {
    featureIndex :: Int64,
    threshold    :: Float64,
    leafValue    :: Int64,
    isLeaf       :: Bool
}

# Module allocations
module NodeStore;
module MLUtils;
module DecisionTreeEngine;

#===============================================================================
# 2. HELPER UTILITIES & METRICS ENGINE
#===============================================================================

# Calculate Rate of Change (Delta) between consecutive steps
fn :: MLUtils calculateDelta(currentMean :: Float64, prevMean :: Float64) -> Float64 {
    return currentMean - prevMean;
}

# Spatial variance across individual ground sensor readings
fn :: MLUtils calculateVariance(sensors :: Array, mean :: Float64) -> Float64 {
    if sensors.size() == 0 { return 0.0; }
    sumSqDiff :: Float64 = 0.0;
    through s :: sensors -> loop {
        diff = float64(s) - mean;
        sumSqDiff = sumSqDiff + (diff * diff);
    }; 
    return sumSqDiff / float64(sensors.size());
}

# Extract feature value dynamically (0 = Mean, 1 = Delta, 2 = Variance)
fn :: MLUtils getFeatureVal(sample :: EarthquakeSample, idx :: Int64) -> Float64 {
    if idx == 0 { return sample.soilMean; }
    if idx == 1 { return sample.soilDelta; }
    return sample.sensorVariance;
}

# Gini Impurity Calculation
fn :: MLUtils giniImpurity(samples :: Array) -> Float64 {
    total = float64(samples.size());
    if total == 0.0 { return 0.0; }

    posCount :: Int64 = 0;
    through s :: samples -> loop {
        if s.target == 1 { posCount++; }
    };

    p1 :: Float64 = float64(posCount) / total;
    p0 :: Float64 = 1.0 - p1;

    return 1.0 - (p1 * p1 + p0 * p0);
}

# Split Dataset By Threshold
fn :: MLUtils splitDataset(samples :: Array, featureIdx :: Int64, threshold :: Float64) -> Array {
    leftSide = [];
    rightSide = [];

    through s :: samples -> loop {
        val :: Float64 = MLUtils.getFeatureVal(s, featureIdx);
        if val <= threshold {
            leftSide.push(s);
        } else {
            rightSide.push(s);
        }
    };

    return [leftSide, rightSide];
}

# Comprehensive Confusion Matrix & Metrics Printer
fn :: MLUtils evaluateMetrics(tp :: Int64, fp :: Int64, tn :: Int64, fn_count :: Int64) {
    total = float64(tp + fp + tn + fn_count);
    acc = float64(tp + tn) / total;

    precision :: Float64 = 0.0;
    if (tp + fp) > 0 { precision = float64(tp) / float64(tp + fp); }

    recall :: Float64 = 0.0;
    if (tp + fn_count) > 0 { recall = float64(tp) / float64(tp + fn_count); }

    f1 :: Float64 = 0.0;
    if (precision + recall) > 0.0 { f1 = 2.0 * (precision * recall) / (precision + recall); }

    out("--- CONFUSION MATRIX ---");
    out("  TP: " + string(tp) + " | FP: " + string(fp));
    out("  FN: " + string(fn_count) + " | TN: " + string(tn));
    out("------------------------");
    out("Accuracy:  " + string(acc * 100.0) + "%");
    out("Precision: " + string(precision));
    out("Recall:    " + string(recall));
    out("F1-Score:  " + string(f1));
}

#===============================================================================
# 3. DECISION TREE TRAINING ENGINE
#===============================================================================

fn :: DecisionTreeEngine buildTree(samples :: Array, depth :: Int64, maxDepth :: Int64, nodeKey :: String) -> TreeNode {
    pos :: Int64 = 0;
    neg :: Int64 = 0;
    through s :: samples -> loop {
        if s.target == 1 { pos++; } else { neg++; }
    };

    majorityLabel :: Int64 = 0;
    if pos >= neg { majorityLabel = 1; }

    # Base cases: Pure node, empty dataset, or maximum depth reached
    if samples.size() == 0 || pos == samples.size() || neg == samples.size() || depth >= maxDepth {
        return TreeNode(-1, 0.0, majorityLabel, true);
    }

    bestGini :: Float64 = 999.0;
    bestFeature :: Int64 = -1;
    bestThreshold :: Float64 = 0.0;
    bestSplit :: Array = [];

    # Iterate through all 3 independent features (0 = Mean, 1 = Delta, 2 = Variance)
    through featureIdx :: 0..2 -> loop {
        through sample :: samples -> loop {
            thresh :: Float64 = MLUtils.getFeatureVal(sample, featureIdx);
            splits = MLUtils.splitDataset(samples, featureIdx, thresh);

            leftSplit  = splits[0];
            rightSplit = splits[1];

            if leftSplit.size() > 0 && rightSplit.size() > 0 {
                wLeft  = float64(leftSplit.size()) / float64(samples.size());
                wRight = float64(rightSplit.size()) / float64(samples.size());

                gini = (wLeft * MLUtils.giniImpurity(leftSplit)) + (wRight * MLUtils.giniImpurity(rightSplit));

                if gini < bestGini {
                    bestGini = gini;
                    bestFeature = featureIdx;
                    bestThreshold = thresh;
                    bestSplit = splits;
                }
            }
        };
    };

    if bestFeature == -1 {
        return TreeNode(-1, 0.0, majorityLabel, true);
    }

    leftKey  = nodeKey + "_L";
    rightKey = nodeKey + "_R";

    leftNode  = DecisionTreeEngine.buildTree(bestSplit[0], depth + 1, maxDepth, leftKey);
    rightNode = DecisionTreeEngine.buildTree(bestSplit[1], depth + 1, maxDepth, rightKey);

    return TreeNode(bestFeature, bestThreshold, -1, false);
}

fn :: DecisionTreeEngine predictSingle(node :: TreeNode, sample :: EarthquakeSample) -> Int64 {
    if node.isLeaf {
        return node.leafValue;
    }

    val :: Float64 = MLUtils.getFeatureVal(sample, node.featureIndex);

    if val <= node.threshold {
        return 0;
    } else {
        return 1;
    }
}

#===============================================================================
# 4. CHRONOLOGICAL DATASET SETUP & MODEL TRAINING
#===============================================================================

out("=== 1. PREPARING TIME-SERIES DATASET ===");

# Chronologically ordered sensor readings (soilMean, soilDelta, sensorVariance, target)
dataset :: Array = [
    EarthquakeSample(2.1,  0.1, 0.05, 0),
    EarthquakeSample(2.3,  0.2, 0.08, 0),
    EarthquakeSample(2.2, -0.1, 0.04, 0),
    EarthquakeSample(3.5,  1.3, 0.45, 0),
    EarthquakeSample(5.8,  2.3, 1.12, 1),
    EarthquakeSample(14.8, 9.0, 3.80, 1),
    EarthquakeSample(16.1, 1.3, 4.10, 1),
    EarthquakeSample(15.2,-0.9, 3.50, 1),
    EarthquakeSample(8.4, -6.8, 1.90, 0),
    EarthquakeSample(3.1, -5.3, 0.30, 0),
    EarthquakeSample(2.8, -0.3, 0.10, 0),
    EarthquakeSample(12.4, 9.6, 2.95, 1),
    EarthquakeSample(15.7, 3.3, 3.40, 1),
    EarthquakeSample(4.2, -11.5,0.60, 0)
];

# Chronological Train/Test Split (First 70% Train, Final 30% Validation)
totalSamples = dataset.size();
splitIndex = int64(float64(totalSamples) * 0.7);

trainData :: Array = [];
testData  :: Array = [];

through i :: 0..(totalSamples - 1) -> loop {
    if i < splitIndex {
        trainData.push(dataset[i]);
    } else {
        testData.push(dataset[i]);
    }
};

out("Total Samples:       " + string(totalSamples));
out("Chronological Train: " + string(trainData.size()) + " (First 70%)");
out("Chronological Test:  " + string(testData.size()) + " (Final 30%)");

out("\n=== 2. TRAINING DECISION TREE MODEL ===");
maxTreeDepth :: Int64 = 3;
modelRoot :: TreeNode = DecisionTreeEngine.buildTree(trainData, 0, maxTreeDepth, "root");
out("Decision Tree Model Trained Successfully!");

#===============================================================================
# 5. MODEL EVALUATION & INFERENCE
#===============================================================================

out("\n=== 3. CHRONOLOGICAL VALIDATION EVALUATION ===");

tp :: Int64 = 0;
fp :: Int64 = 0;
tn :: Int64 = 0;
fn_count :: Int64 = 0;

through testSample :: testData -> loop {
    pred = DecisionTreeEngine.predictSingle(modelRoot, testSample);
    
    if pred == 1 && testSample.target == 1 { tp++; }
    if pred == 1 && testSample.target == 0 { fp++; }
    if pred == 0 && testSample.target == 0 { tn++; }
    if pred == 0 && testSample.target == 1 { fn_count++; }
};

MLUtils.evaluateMetrics(tp, fp, tn, fn_count);

out("\n=== 4. INFERENCE ENGINE ===");

# Test input representing a rapid spike (high mean, high delta, high sensor variance)
unseenSample :: EarthquakeSample = EarthquakeSample(14.5, 8.2, 3.10, 0);

prediction :: Int64 = DecisionTreeEngine.predictSingle(modelRoot, unseenSample);

out("Sample Input - Soil Mean: " + string(unseenSample.soilMean) + 
    ", Delta: " + string(unseenSample.soilDelta) + 
    ", Sensor Variance: " + string(unseenSample.sensorVariance));

if prediction == 1 {
    out("High probability of an earthquake occurring within the next 24 hours.");
} else {
    out("Low probability of an earthquake occurring within the next 24 hours.");
}