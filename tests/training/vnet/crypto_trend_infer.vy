#===============================================================================
# VYNENET CRYPTO INFERENCE (STABLE VERSION)
#===============================================================================

ruleset { dynamic_casting };

use lib "vml.vy";
use lib "vcolors.vy";
use lib "vlinalg.vy";

module vcore;
module vfs;

out(vcolors.cyan("=== VyneNet Rapid Inference: Crypto Analysis ==="));

# 1. Helper funksiyasını bərpa edirik (CSV datası üçün)
fn clean_num(val) -> Float64 {
    s = string(val);
    t = s.replace("$", "").replace("%", "").replace(",", "").replace("\"", "").replace(" ", "");
    if t == "" || t == "-" || t == "$-" { return 0.0; }
    return float64(t);
}

# 2. Modeli Diskdən Yükləyirik

w1_raw = vfs.read("models/crypto_market_v1/w1.dat");
b1_raw = vfs.read("models/crypto_market_v1/b1.dat");
w2_raw = vfs.read("models/crypto_market_v1/w2.dat");
b2_raw = vfs.read("models/crypto_market_v1/b2.dat");

w1_data = vcore.parse_array(w1_raw);
b1_data = vcore.parse_array(b1_raw);
w2_data = vcore.parse_array(w2_raw);
b2_data = vcore.parse_array(b2_raw);

# Matrix ölçüləri: Input(3) -> Hidden(12) -> Output(1)
h_layer = vml.Types.Layer(vlinalg.Types.Matrix(3, 12, w1_data), b1_data);
o_layer = vml.Types.Layer(vlinalg.Types.Matrix(12, 1, w2_data), b2_data);

# lr 0.0 qoyuruq, çünki öyrənmə (training) etməyəcəyik
crypto_net = vml.Types.NeuralNet(h_layer, o_layer, 0.0);

out(vcolors.green("Model loaded. Weights synchronized."));

# 3. Datanı yükləyirik və proqnoz veririk
raw_data = vfs.parse_csv("crypto.csv");

out("\n" + vcolors.cyan("--- Market Sentiment Analysis ---"));

# İlk 10 coin-i analiz edək
through k :: 0..9 -> loop {
    row = raw_data[k+1];
    coin_name = row[1];
    
    # Inputları hazırlayırıq (h1, h24, d7)
    h1  = clean_num(row[4]) / 100.0;
    h24 = clean_num(row[5]) / 100.0;
    d7  = clean_num(row[6]) / 100.0;

    X = vlinalg.Types.Matrix(1, 3, [[h1, h24, d7]]);
    
    # Sənin vml.vy daxilindəki predict funksiyanı çağırırıq
    result = vml.predict(crypto_net, X);
    score = result.data[0][0] * 100;
    
    msg = coin_name + " Score: " + string(score) + "%";
    
    if score > 65 {
        out(vcolors.green(msg + " [ BULLISH ]"));
    } else if score < 35 {
        out(vcolors.red(msg + " [ BEARISH ]"));
    } else {
        out(vcolors.yellow(msg + " [ NEUTRAL ]"));
    }
}