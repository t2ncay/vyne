ruleset { dynamic_casting };

use lib "vcolors.vy";
use lib "vlinalg.vy";
module vmath;
module vmem;

#===============================================================================
# 1. DATA VƏ PARAMETRLƏR
#===============================================================================
X :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(4, 2, [
    [0.0, 0.0], [0.0, 1.0], [1.0, 0.0], [1.0, 1.0]
]);

Y :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(4, 1, [
    [0.0], [1.0], [1.0], [0.0]
]);

# Çəkiləri -1.0 və 1.0 arasında random başladırıq
W1 :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(2, 2, [
    [(vmath.random(0, 20)/10.0)-1.0, (vmath.random(0, 20)/10.0)-1.0],
    [(vmath.random(0, 20)/10.0)-1.0, (vmath.random(0, 20)/10.0)-1.0]
]);

W2 :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(2, 1, [
    [(vmath.random(0, 20)/10.0)-1.0],
    [(vmath.random(0, 20)/10.0)-1.0]
]);

B1 :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(1, 2, [[0.1, 0.1]]);
B2 :: vlinalg.Types.Matrix = vlinalg.Types.Matrix(1, 1, [[0.1]]);

learning_rate = 0.5;
epochs = 3000;

#===============================================================================
# 2. TRAINING SESSION (Xətasız Versiya)
#===============================================================================
through epoch :: 1..epochs -> loop {
    # --- Forward Pass ---
    Z1 = vlinalg.multiply(X, W1);

    # ridx və cidx-i birbaşa through ilə təyin edirik
    through ridx :: 0..Z1.row-1 -> loop {
        through cidx :: 0..Z1.col-1 -> loop {
            Z1.data[ridx][cidx] = Z1.data[ridx][cidx] + B1.data[0][cidx];
        };
    };
    A1 = vlinalg.apply_sigmoid(Z1);
    
    Z2 = vlinalg.multiply(A1, W2);
    through r :: 0..Z2.row-1 -> loop {
        Z2.data[r][0] = Z2.data[r][0] + B2.data[0][0];
    };
    A2 = vlinalg.apply_sigmoid(Z2); 
    
    # --- Backpropagation ---
    error_out = vlinalg.subtract(Y, A2);
    
    through row_idx :: 0..A1.row-1 -> loop {
        e_val = error_out.data[row_idx][0];
        delta_out = e_val * (A2.data[row_idx][0] * (1.0 - A2.data[row_idx][0]));
        
        B2.data[0][0] = B2.data[0][0] + (learning_rate * delta_out);
        
        through w_idx :: 0..W2.row-1 -> loop {
            W2.data[w_idx][0] = W2.data[w_idx][0] + (learning_rate * delta_out * A1.data[row_idx][w_idx]);
        };
    };

    through row_idx :: 0..X.row-1 -> loop {
        through h_idx :: 0..W1.col-1 -> loop {
            h_err = error_out.data[row_idx][0] * W2.data[h_idx][0];
            h_delta = h_err * (A1.data[row_idx][h_idx] * (1.0 - A1.data[row_idx][h_idx]));
            
            B1.data[0][h_idx] = B1.data[0][h_idx] + (learning_rate * h_delta);

            through in_idx :: 0..W1.row-1 -> loop {
                W1.data[in_idx][h_idx] = W1.data[in_idx][h_idx] + (learning_rate * h_delta * X.data[row_idx][in_idx]);
            };
        };
    };

    # --- Loss Calculation (MSE) ---
    # Hər epoch-da səhvlərin kvadratının ortalamasını tapaq
    current_loss = 0.0;
    through loss_idx :: 0..error_out.row-1 -> loop {
        err = error_out.data[loss_idx][0];
        current_loss = current_loss + (err * err);
    };
    avg_loss = current_loss / float64(error_out.row);

    # Log hissəsini belə yenilə:
    if epoch % 200 == 0 {
        log_msg = "Epoch " + string(epoch);
        log_msg = log_msg + " | Loss: " + string(avg_loss);
        log_msg = log_msg + " | Memory: " + string(vmem.usage()) + " bytes";
        
        out(vcolors.green(log_msg));
    }
};

#===============================================================================
# 3. NƏTİCƏ
#===============================================================================
out(vcolors.success("Training complete. Analyzing XOR logic..."));

# Final test üçün də bias əlavə etmək lazımdır
fz1 = vlinalg.multiply(X, W1);
through r :: 0..fz1.row-1 -> loop {
    through c :: 0..fz1.col-1 -> loop {
        fz1.data[r][c] = fz1.data[r][c] + B1.data[0][c];
    };
};
fa1 = vlinalg.apply_sigmoid(fz1);
fz2 = vlinalg.multiply(fa1, W2);
through r :: 0..fz2.row-1 -> loop { fz2.data[r][0] = fz2.data[r][0] + B2.data[0][0]; };
fa2 = vlinalg.apply_sigmoid(fz2);

through i :: 0..3 -> loop {
    out("Input: " + string(X.data[i]) + " -> Result: " + string(fa2.data[i][0]));
};