ruleset { dynamic_casting };

use lib "vml.vy";
use lib "vcolors.vy";
use lib "vlinalg.vy";

module vcore;
module vfs;

out(vcolors.cyan("=== VyneNet Big Data Intelligence Mode ==="));

# 1. Dataset-in Yüklənməsi
out("Loading full crypto dataset...");
raw_data = vfs.parse_csv("crypto.csv");
out("Total rows loaded: " + string(raw_data.size()));

# Təmizləmə funksiyası (Artıq stabil işləyir)
fn clean_num(val) -> Float64 {
    s = string(val);
    t = s.replace("$", "").replace("%", "").replace(",", "").replace("\"", "").replace(" ", "");
    if t == "" || t == "-" || t == "$-" { return 0.0; }
    return float64(t);
}

inputs_list = [];
targets_list = [];

# Nə qədər data ilə öyrənəcəyimizi seçirik (Məsələn: Top 500)
train_limit = 500;
if raw_data.size() < train_limit { train_limit = raw_data.size(); }

out("Preprocessing top " + string(train_limit) + " assets...");

through i :: 1..train_limit-1 -> loop {
    row = raw_data[i];
    
    # 1h, 24h, 7d sütunlarını (idx 4,5,6) götürürük
    if row.size() >= 8 && row[4] != "-" && row[5] != "-" && row[6] != "-" {
        
        h1  = clean_num(row[4]) / 100.0;
        h24 = clean_num(row[5]) / 100.0;
        d7  = clean_num(row[6]) / 100.0;
        d30 = clean_num(row[7]); # Target (30 günlük trend)

        inputs_list.push([h1, h24, d7]);
        
        # Bullish (1) yoxsa Bearish (0)
        target_val = 0.0;
        if d30 > 0 { target_val = 1.0; }
        targets_list.push([target_val]);
    }
};

# 2. Matrix Hazırlığı
X = vlinalg.Types.Matrix(inputs_list.size(), 3, inputs_list);
Y = vlinalg.Types.Matrix(targets_list.size(), 1, targets_list);

# 3. Neural Network (3 input, 12 hidden, 1 output)
# Data çoxaldığı üçün hidden layer-i bir az böyütdük (12 neyron)
crypto_net = vml.create_network(3, 12, 1, 0.02);

epochs = 10000;
out("Training on " + string(inputs_list.size()) + " assets for " + string(epochs) + " epochs...");

through e :: 1..epochs -> loop {
    crypto_net = vml.train_step(crypto_net, X, Y);
    
    if e % 5000 == 0 {
        out(vcolors.yellow("Checkpoint: Epoch " + string(e) + " completed."));
    }
};

out(vcolors.green("VyneNet is fully trained and ready for inference!"));

# 4. Real-time Prediction (Top 10 Coins)
out("\n" + vcolors.cyan("--- Market Sentiment Analysis ---"));

through k :: 0..9 -> loop {
    coin_name = raw_data[k+1][1];
    test_input = vlinalg.Types.Matrix(1, 3, [inputs_list[k]]);
    
    prediction = vml.predict(crypto_net, test_input);
    score = prediction.data[0][0] * 100;
    
    msg = coin_name + " Trend Score: " + string(score) + "%";
    
    if score > 65 {
        out(vcolors.green(msg + " [ BULLISH ]"));
    } else if score < 35 {
        out(vcolors.red(msg + " [ BEARISH ]"));
    } else {
        out(vcolors.yellow(msg + " [ NEUTRAL ]"));
    }
};