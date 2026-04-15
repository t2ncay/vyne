#===============================================================================
# VYNENET (INFERENCE ONLY)
#===============================================================================

ruleset { dynamic_casting };

use lib "vlinalg.vy"
module vfs;
module vcore;

interface Layer {
    weights :: vlinalg.Types.Matrix,
    biases  :: Array
}

interface NeuralNet {
    hidden  :: Layer,
    output  :: Layer,
    lr      :: Float64
}

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

w1_raw = vfs.read("xor_trained_v1/w1.dat");
b1_raw = vfs.read("xor_trained_v1/b1.dat");
w2_raw = vfs.read("xor_trained_v1/w2.dat");
b2_raw = vfs.read("xor_trained_v1/b2.dat");

w1_data = vcore.parse_array(w1_raw);
b1_data = vcore.parse_array(b1_raw);
w2_data = vcore.parse_array(w2_raw);
b2_data = vcore.parse_array(b2_raw);

h_layer = Layer(vlinalg.Types.Matrix(2, 4, w1_data), b1_data);
o_layer = Layer(vlinalg.Types.Matrix(4, 1, w2_data), b2_data);

my_trained_net = NeuralNet(h_layer, o_layer, 0.0); # lr artıq lazım deyil

out(vcolors.cyan("VyneNet resurrected. Testing with zero training..."));

test_val = vlinalg.Types.Matrix(1, 2, [[1, 0]]);
result = predict(my_trained_net, test_val);

out("Input: [1, 0] -> Predict: " + string(result.data[0][0]));