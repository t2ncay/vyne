#===============================================================================
# VML - VYNE MACHINE LEARNING LIBRARY (FULL CUMULATIVE VERSION)
#===============================================================================

ruleset { dynamic_casting };

module vml;

use lib "vlinalg.vy";

#===============================================================================
# VML TYPES & INTERFACES
#===============================================================================

group Types :: vml {
    interface Layer {
        weights :: vlinalg.Types.Matrix,
        biases  :: Array
    }

    # Standard architecture: Input -> Hidden -> Output
    interface NeuralNet {
        hidden  :: Layer,
        output  :: Layer,
        lr      :: Float64
    }

    # Deep architecture: Input -> Hidden1 -> Hidden2 -> Output
    interface DeepNeuralNet {
        hidden1 :: Layer,
        hidden2 :: Layer,
        output  :: Layer,
        lr      :: Float64
    }
};

#===============================================================================
# VML CORE FUNCTIONS (STANDARD)
#===============================================================================

fn :: vml predict(net :: vml.Types.NeuralNet, X :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    h_z = vlinalg.add_bias(vlinalg.multiply(X, net.hidden.weights), net.hidden.biases);
    h_a = vlinalg.apply_sigmoid(h_z);

    o_z = vlinalg.add_bias(vlinalg.multiply(h_a, net.output.weights), net.output.biases);
    return vlinalg.apply_sigmoid(o_z);
}

fn :: vml train_step(net :: vml.Types.NeuralNet, X :: vlinalg.Types.Matrix, Y :: vlinalg.Types.Matrix) -> vml.Types.NeuralNet {
    # 1. Forward
    h_z = vlinalg.add_bias(vlinalg.multiply(X, net.hidden.weights), net.hidden.biases);
    h_a = vlinalg.apply_sigmoid(h_z);
    o_z = vlinalg.add_bias(vlinalg.multiply(h_a, net.output.weights), net.output.biases);
    o_a = vlinalg.apply_sigmoid(o_z);

    # 2. Backpropagation
    error_o = vlinalg.subtract(o_a, Y); 
    delta_o = vlinalg.hadamard(error_o, vlinalg.sigmoid_prime(o_a));

    w2_t = vlinalg.transpose(net.output.weights);
    delta_h = vlinalg.hadamard(vlinalg.multiply(delta_o, w2_t), vlinalg.sigmoid_prime(h_a));

    # 3. Update Biases
    new_b_o = through c :: 0..net.output.weights.col-1 -> collect {
        sum :: Float64 = 0.0;
        through r :: 0..delta_o.row-1 -> loop { sum = sum + delta_o.data[r][c]; };
        net.output.biases[c] - (net.lr * sum)
    };

    new_b_h = through c :: 0..net.hidden.weights.col-1 -> collect {
        sum :: Float64 = 0.0;
        through r :: 0..delta_h.row-1 -> loop { sum = sum + delta_h.data[r][c]; };
        net.hidden.biases[c] - (net.lr * sum)
    };

    # 4. Update Weights
    new_w2 = vlinalg.subtract(net.output.weights, vlinalg.multiply_scalar(vlinalg.multiply(vlinalg.transpose(h_a), delta_o), net.lr));
    new_w1 = vlinalg.subtract(net.hidden.weights, vlinalg.multiply_scalar(vlinalg.multiply(vlinalg.transpose(X), delta_h), net.lr));

    return vml.Types.NeuralNet(vml.Types.Layer(new_w1, new_b_h), vml.Types.Layer(new_w2, new_b_o), net.lr);
}

fn :: vml create_network(input_n :: Int64, hidden_n :: Int64, output_n :: Int64, lr :: Float64) -> vml.Types.NeuralNet {
    h_layer = vml.Types.Layer(vlinalg.random_init(input_n, hidden_n), through 0..hidden_n-1 -> collect { 0.1 });
    o_layer = vml.Types.Layer(vlinalg.random_init(hidden_n, output_n), through 0..output_n-1 -> collect { 0.1 });
    return vml.Types.NeuralNet(h_layer, o_layer, lr);
}

#===============================================================================
# VML DEEP FUNCTIONS (3 -> 8 -> 4 -> 1)
#===============================================================================

fn :: vml predict_deep(net :: vml.Types.DeepNeuralNet, X :: vlinalg.Types.Matrix) -> vlinalg.Types.Matrix {
    h1_a = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(X, net.hidden1.weights), net.hidden1.biases));
    h2_a = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(h1_a, net.hidden2.weights), net.hidden2.biases));
    return vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(h2_a, net.output.weights), net.output.biases));
}

fn :: vml train_deep_step(net :: vml.Types.DeepNeuralNet, X :: vlinalg.Types.Matrix, Y :: vlinalg.Types.Matrix) -> vml.Types.DeepNeuralNet {
    # 1. Forward Pass
    h1_a = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(X, net.hidden1.weights), net.hidden1.biases));
    h2_a = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(h1_a, net.hidden2.weights), net.hidden2.biases));
    o_a  = vlinalg.apply_sigmoid(vlinalg.add_bias(vlinalg.multiply(h2_a, net.output.weights), net.output.biases));

    # 2. Backpropagation
    err_o = vlinalg.subtract(o_a, Y);
    del_o = vlinalg.hadamard(err_o, vlinalg.sigmoid_prime(o_a));

    del_h2 = vlinalg.hadamard(vlinalg.multiply(del_o, vlinalg.transpose(net.output.weights)), vlinalg.sigmoid_prime(h2_a));
    del_h1 = vlinalg.hadamard(vlinalg.multiply(del_h2, vlinalg.transpose(net.hidden2.weights)), vlinalg.sigmoid_prime(h1_a));

    # 3. Update Biases (pattern matched to your loop logic)
    new_b_o = through c :: 0..net.output.biases.size()-1 -> collect {
        sum = 0.0; through r :: 0..del_o.row-1 -> loop { sum = sum + del_o.data[r][c]; };
        net.output.biases[c] - (net.lr * sum)
    };
    new_b_h2 = through c :: 0..net.hidden2.biases.size()-1 -> collect {
        sum = 0.0; through r :: 0..del_h2.row-1 -> loop { sum = sum + del_h2.data[r][c]; };
        net.hidden2.biases[c] - (net.lr * sum)
    };
    new_b_h1 = through c :: 0..net.hidden1.biases.size()-1 -> collect {
        sum = 0.0; through r :: 0..del_h1.row-1 -> loop { sum = sum + del_h1.data[r][c]; };
        net.hidden1.biases[c] - (net.lr * sum)
    };

    # 4. Update Weights
    new_w_o  = vlinalg.subtract(net.output.weights, vlinalg.multiply_scalar(vlinalg.multiply(vlinalg.transpose(h2_a), del_o), net.lr));
    new_w_h2 = vlinalg.subtract(net.hidden2.weights, vlinalg.multiply_scalar(vlinalg.multiply(vlinalg.transpose(h1_a), del_h2), net.lr));
    new_w_h1 = vlinalg.subtract(net.hidden1.weights, vlinalg.multiply_scalar(vlinalg.multiply(vlinalg.transpose(X), del_h1), net.lr));

    return vml.Types.DeepNeuralNet(vml.Types.Layer(new_w_h1, new_b_h1), vml.Types.Layer(new_w_h2, new_b_h2), vml.Types.Layer(new_w_o, new_b_o), net.lr);
}

fn :: vml create_deep_network(in_n :: Int64, h1_n :: Int64, h2_n :: Int64, out_n :: Int64, lr :: Float64) -> vml.Types.DeepNeuralNet {
    h1 = vml.Types.Layer(vlinalg.random_init(in_n, h1_n), through 0..h1_n-1 -> collect { 0.1 });
    h2 = vml.Types.Layer(vlinalg.random_init(h1_n, h2_n), through 0..h2_n-1 -> collect { 0.1 });
    o  = vml.Types.Layer(vlinalg.random_init(h2_n, out_n), through 0..out_n-1 -> collect { 0.1 });
    return vml.Types.DeepNeuralNet(h1, h2, o, lr);
}

#===============================================================================
# I/O OPERATIONS
#===============================================================================

fn :: vml save_model(net :: vml.Types.NeuralNet, path :: String) {
    vfs.mkdir(path, true);
    vfs.write(vfs.join(path, "w1.dat"), string(net.hidden.weights.data));
    vfs.write(vfs.join(path, "b1.dat"), string(net.hidden.biases));
    vfs.write(vfs.join(path, "w2.dat"), string(net.output.weights.data));
    vfs.write(vfs.join(path, "b2.dat"), string(net.output.biases));
}

fn :: vml save_deep_model(net :: vml.Types.DeepNeuralNet, path :: String) {
    vfs.mkdir(path, true);
    vfs.write(vfs.join(path, "w1.dat"), string(net.hidden1.weights.data));
    vfs.write(vfs.join(path, "b1.dat"), string(net.hidden1.biases));
    vfs.write(vfs.join(path, "w2.dat"), string(net.hidden2.weights.data));
    vfs.write(vfs.join(path, "b2.dat"), string(net.hidden2.biases));
    vfs.write(vfs.join(path, "w3.dat"), string(net.output.weights.data));
    vfs.write(vfs.join(path, "b3.dat"), string(net.output.biases));
}

deploy vml;