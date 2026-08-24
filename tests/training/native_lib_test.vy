ruleset { dynamic_casting };

module vml;
module vmath;

# Create a simple XOR model
model = vml.sequential("xor");
model.add(vml.dense(2, 4, "relu"));
model.add(vml.dense(4, 1, "sigmoid"));

# XOR training data
X = vml.tensor(4, 2);
X[0][0] = 0; X[0][1] = 0;
X[1][0] = 0; X[1][1] = 1;
X[2][0] = 1; X[2][1] = 0;
X[3][0] = 1; X[3][1] = 1;

y = vml.tensor(4, 1);
y[0][0] = 0;
y[1][0] = 1;
y[2][0] = 1;
y[3][0] = 0;

# Train
vml.train(model, X, y, 0.1, 1000, false);

# Predict
pred = vml.predict(model, X);

# Output results
out("Results:");
through i :: 0..3 -> loop {
    rounded = vmath.round(pred[i][0]);
    out(string(X[i][0]) + " XOR " + string(X[i][1]) + " = " + string(rounded));
};

out("Loss: " + string(vml.mse(pred, y)));