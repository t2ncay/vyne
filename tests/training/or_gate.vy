ruleset { 
    dynamic_casting
};

use lib "vcolors.vy";
use lib "vlinalg.vy";

module vmath;
module vmem;

#===============================================================================
# DATA AND PARAMETERS (OR GATE)
#===============================================================================

out(vcolors.boldYellow(" --- Vyne Brain: Training Session --- "));

# OR Gate Girişləri - Tip deklarasiyası düzəldildi
X :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(4, 2, [
    [0.0, 0.0],
    [0.0, 1.0],
    [1.0, 0.0],
    [1.0, 1.0]
]);

# Target outputs
Y :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(4, 1, [
    [0.0],
    [1.0],
    [1.0],
    [1.0]
]);

# Weights - W :: vlinalg.Types.Matrix
W :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(2, 1, [
    [vmath.random(0, 10) / 10.0],
    [vmath.random(0, 10) / 10.0]
]);

learning_rate = 0.1;
epochs = 200;

#===============================================================================
# TRAINING LOOP (GRADIENT DESCENT)
#===============================================================================

through epoch :: 1..epochs -> loop {
    # Forward Pass
    Z = vlinalg.multiply(X, W);
    Y_hat = vlinalg.apply_sigmoid(Z);
    
    # Error Calculation (E = Y - Y_hat)
    error = vlinalg.subtract(Y, Y_hat);
    
    # Gradient Update (Delta Rule)
    through r :: 0..X.row-1 -> loop {
        row_idx = r;
        err = error.data[row_idx][0];
        
        through c :: 0..W.row-1 -> loop {
            col_idx = c;
            input_val = X.data[row_idx][col_idx];
            
            # W.data birbaşa referans vasitəsilə yenilənir
            W.data[col_idx][0] = W.data[col_idx][0] + (learning_rate * err * input_val);
        };
    };

    if epoch % 50 == 0 {
        out(vcolors.green("Epoch " + string(epoch) + " completed."));
    }
};

out(vcolors.success("Vyne has successfully learned the OR Gate!"));

#===============================================================================
# TESTING & MEMORY ANALYSIS
#===============================================================================

out("\n" + vcolors.boldGreen("Final Predictions:"));
final_z = vlinalg.multiply(X, W);
predictions = vlinalg.apply_sigmoid(final_z);

through i :: 0..3 -> loop {
    idx = i;
    in_1 = string(X.data[idx][0]);
    in_2 = string(X.data[idx][1]);
    pred = string(predictions.data[idx][0]);
    
    out("Input: [" + in_1 + ", " + in_2 + "] -> AI Output: " + pred);
};

# Yaddaş analizi üçün vmem modulu çağırılır
out("\n" + vcolors.bgRed(" Memory Footprint: " + string(vmem.usage()) + " bytes "));