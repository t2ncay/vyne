#===============================================================================
# VYNENET DEEP INFERENCE (3 -> 8 -> 4 -> 1 Architecture)
#===============================================================================

ruleset { dynamic_casting };

use lib "vml.vy";
use lib "vcolors.vy";
use lib "vlinalg.vy";

module vcore;
module vfs;

out(vcolors.cyan("=== VyneNet Deep Inference: Balanced Sentiment Analysis ==="));

fn clean_num(val) -> Float64 {
    s = string(val);
    t = s.replace("$", "").replace("%", "").replace(",", "").replace("\"", "").replace(" ", "");
    if t == "" || t == "-" || t == "$-" { return 0.0; }
    return float64(t);
}

w1 = vcore.parse_array(vfs.read("models/crypto_deep_v1/w1.dat"));
b1 = vcore.parse_array(vfs.read("models/crypto_deep_v1/b1.dat"));
w2 = vcore.parse_array(vfs.read("models/crypto_deep_v1/w2.dat"));
b2 = vcore.parse_array(vfs.read("models/crypto_deep_v1/b2.dat"));
w3 = vcore.parse_array(vfs.read("models/crypto_deep_v1/w3.dat"));
b3 = vcore.parse_array(vfs.read("models/crypto_deep_v1/b3.dat"));

# Matrix ölçülərini memarlığa uyğun qururuq: 3 -> 8 -> 4 -> 1
h1_layer = vml.Types.Layer(vlinalg.Types.Matrix(3, 8, w1), b1);
h2_layer = vml.Types.Layer(vlinalg.Types.Matrix(8, 4, w2), b2);
o_layer  = vml.Types.Layer(vlinalg.Types.Matrix(4, 1, w3), b3);

# DeepNeuralNet obyektini yaradırıq
crypto_net = vml.Types.DeepNeuralNet(h1_layer, h2_layer, o_layer, 0.0);

out(vcolors.green("Deep Model loaded. 3-layer architecture synchronized."));

# 2. Datanı yükləyirik
raw_data = vfs.parse_csv("crypto.csv");

out("\n" + vcolors.cyan("--- Balanced Market Sentiment Analysis ---"));

through k :: 0..9 -> loop {
    row = raw_data[k+1];
    coin_name = row[1];
    
    # Input Normalization (Training-də olduğu kimi 100-ə bölürük)
    h1  = clean_num(row[4]) / 100.0;
    h24 = clean_num(row[5]) / 100.0;
    d7  = clean_num(row[6]) / 100.0;

    X = vlinalg.Types.Matrix(1, 3, [[h1, h24, d7]]);
    
    # DİQQƏT: predict_deep funksiyasını çağırırıq
    result = vml.predict_deep(crypto_net, X);
    score = result.data[0][0] * 100;
    
    msg = coin_name + " Trend Score: " + string(score) + "%";
    
    if score > 60 {
        out(vcolors.green(msg + " [ BULLISH ]"));
    } else if score < 40 {
        out(vcolors.red(msg + " [ BEARISH ]"));
    } else {
        out(vcolors.yellow(msg + " [ NEUTRAL ]"));
    }
}