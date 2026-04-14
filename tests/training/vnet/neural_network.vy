#===============================================================================
# VYNENET - 3-LAYER NEURAL NETWORK
#===============================================================================

ruleset { dynamic_casting };

module vcore;
module vfs;

module vnnet;

use lib "vlinalg.vy"

interface Layer {
    weights :: vlinalg.Types.Matrix,
    biases  :: Array
}

interface NeuralNet {
    hidden  :: Layer,
    output  :: Layer,
    lr      :: Float64
}

# --- Forward Pass (Prediction) ---
fn predict(net :: NeuralNet, X :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    # 1. Hidden Layer: activation = sigmoid(X * W1 + b1)
    h_z = vlinalg.multiply(X, net.hidden.weights);
    h_z_biased = vlinalg.add_bias(h_z, net.hidden.biases);
    h_a = vlinalg.apply_sigmoid(h_z_biased);

    # 2. Output Layer: activation = sigmoid(h_a * W2 + b2)
    o_z = vlinalg.multiply(h_a, net.output.weights);
    o_z_biased = vlinalg.add_bias(o_z, net.output.biases);
    o_a = vlinalg.apply_sigmoid(o_z_biased);

    return o_a;
}

fn save_model(net :: NeuralNet, model_name :: String) {
    vfs.mkdir(model_name, true);
    
    vfs.write(vfs.join(model_name, "w1.dat"), string(net.hidden.weights.data));
    vfs.write(vfs.join(model_name, "b1.dat"), string(net.hidden.biases));
    
    vfs.write(vfs.join(model_name, "w2.dat"), string(net.output.weights.data));
    vfs.write(vfs.join(model_name, "b2.dat"), string(net.output.biases));
    
    out(vcolors.green("Model saved successfully to: " + model_name));
}

# --- 1. Initialization ---
fn create_network(input_n, hidden_n, output_n, learning_rate) -> NeuralNet {
    h_layer = Layer(
        vlinalg.random_init(input_n, hidden_n),
        through 0..hidden_n-1 -> collect { 0.1 } # Initial biases
    );
    
    o_layer = Layer(
        vlinalg.random_init(hidden_n, output_n),
        through 0..output_n-1 -> collect { 0.1 }
    );

    return NeuralNet(h_layer, o_layer, learning_rate);
}

# --- 2. Training Step (Backpropagation) ---
fn train_step(net :: NeuralNet, X :: vlinalg.Types.Matrix, Y :: vlinalg.Types.Matrix) -> NeuralNet {
    # --- FORWARD PASS ---
    h_z = vlinalg.add_bias(vlinalg.multiply(X, net.hidden.weights), net.hidden.biases);
    h_a = vlinalg.apply_sigmoid(h_z);

    o_z = vlinalg.add_bias(vlinalg.multiply(h_a, net.output.weights), net.output.biases);
    o_a = vlinalg.apply_sigmoid(o_z);

    error_o = vlinalg.subtract(o_a, Y); 
    delta_o = vlinalg.hadamard(error_o, vlinalg.sigmoid_prime(o_a));

    w2_t = vlinalg.transpose(net.output.weights);
    error_h = vlinalg.multiply(delta_o, w2_t);
    delta_h = vlinalg.hadamard(error_h, vlinalg.sigmoid_prime(h_a));

    # --- GRADIENTS ---
    h_a_t = vlinalg.transpose(h_a);
    dw2 = vlinalg.multiply(h_a_t, delta_o);
    
    x_t = vlinalg.transpose(X);
    dw1 = vlinalg.multiply(x_t, delta_h);

    # Output Layer Bias Update
    new_bias_o = through c :: 0..net.output.weights.col-1 -> collect {
        sum :: Float64 = 0.0;
        through r :: 0..delta_o.row-1 -> loop { sum = sum + delta_o.data[r][c]; };
        net.output.biases[c] - (net.lr * sum) # Bu düzdür
    };

    # Hidden Layer Bias Update
    new_bias_h = through c :: 0..net.hidden.weights.col-1 -> collect {
        sum :: Float64 = 0.0;
        through r :: 0..delta_h.row-1 -> loop { sum = sum + delta_h.data[r][c]; };
        # BURADA: net.output.biases yox, net.hidden.biases olmalıdır!
        net.hidden.biases[c] - (net.lr * sum) 
    };

    # --- WEIGHT UPDATES (Subtract istifadə edirik) ---
    new_w2 = vlinalg.subtract(net.output.weights, vlinalg.multiply_scalar(dw2, net.lr));
    new_w1 = vlinalg.subtract(net.hidden.weights, vlinalg.multiply_scalar(dw1, net.lr));

    return NeuralNet(
        Layer(new_w1, new_bias_h),
        Layer(new_w2, new_bias_o),
        net.lr
    );
}

# --- 3. Execution ---

out(vcolors.green("VyneNet Neural Training Starting..."));

# Data: XOR pattern (Input: 2, Output: 1)
inputs  = vlinalg.Types.Matrix(4, 2, [[0,0], [0,1], [1,0], [1,1]]);
targets = vlinalg.Types.Matrix(4, 1, [[0], [1], [1], [0]]);

# Create 2-4-1 Network
my_net = create_network(2, 4, 1, 0.1);

# Training Loop
epochs = 50000;
through i :: 1..epochs -> loop {
    my_net = train_step(my_net, inputs, targets);
    
    if i % 200 == 0 {
        out("Epoch " + string(i) + " completed.");
    }
};

out(vcolors.green("Training finished. Vyne is officially 'intelligent' now."));

save_model(my_net, "xor_trained_v1");

# --- 4. Testing / Inference ---
out("\n--- Testing VyneNet Predictions ---");

test_cases = [[0, 0], [0, 1], [1, 0], [1, 1]];

through input_vec :: test_cases -> loop {
    X_test = vlinalg.Types.Matrix(1, 2, [input_vec]);
    
    prediction = predict(my_net, X_test);
    
    res_val = prediction.data[0][0]; # İlk (və tək) nəticə
    
    out("Input: " + string(input_vec) + " -> Output: " + string(res_val));
    
    # İnsanca şərh edək
    if res_val > 0.8 {
        out(vcolors.green("   Result: TRUE (High Confidence)"));
    } else if res_val < 0.2 {
        out(vcolors.red("   Result: FALSE (High Confidence)"));
    } else {
        out("   Result: NOT SURE (Thinking...)");
    }
    out("---------------------------");
};