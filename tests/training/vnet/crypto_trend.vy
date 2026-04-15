ruleset { dynamic_casting };
use lib "vml.vy";
use lib "vcolors.vy";
use lib "vlinalg.vy";

module vcore;
module vfs;

out(vcolors.cyan("=== VyneNet Deep Intelligence: Balanced Mode ==="));

raw_data = vfs.parse_csv("crypto.csv");
inputs_list = [];
targets_list = [];

# BALANS ÜÇÜN COUNTERLƏR
bull_count = 0;
bear_count = 0;
max_samples = 150; # Hər tərəfdən 150 nümunə (cəmi 300)

fn clean_num(val) -> Float64 {
    s = string(val);
    t = s.replace("$", "").replace("%", "").replace(",", "").replace("\"", "").replace(" ", "");
    if t == "" || t == "-" || t == "$-" { return 0.0; }
    return float64(t);
}

through i :: 1..raw_data.size()-1 -> loop {
    row = raw_data[i];
    if row.size() >= 8 && row[4] != "-" && row[5] != "-" {
        
        # NORMALIZATION: Qiymətləri [-1, 1] arasına sıxırıq
        h1  = clean_num(row[4]) / 100.0;
        h24 = clean_num(row[5]) / 100.0;
        d7  = clean_num(row[6]) / 100.0;
        d30 = clean_num(row[7]);

        is_bullish = d30 > 0;

        # DATA BALANCING LOGIC
        if is_bullish && bull_count < max_samples {
            inputs_list.push([h1, h24, d7]);
            targets_list.push([1.0]);
            bull_count = bull_count + 1;
        } else if !is_bullish && bear_count < max_samples {
            inputs_list.push([h1, h24, d7]);
            targets_list.push([0.0]);
            bear_count = bear_count + 1;
        }
    }
};

X = vlinalg.Types.Matrix(inputs_list.size(), 3, inputs_list);
Y = vlinalg.Types.Matrix(targets_list.size(), 1, targets_list);

# YENİ LEARNING RATE VƏ MEMARI (3 -> 8 -> 4 -> 1)
lr = 0.005;
crypto_net = vml.create_deep_network(3, 8, 4, 1, lr);

epochs = 30000;
out("Training Deep Vyne on " + string(inputs_list.size()) + " balanced assets...");

through e :: 1..epochs -> loop {
    crypto_net = vml.train_deep_step(crypto_net, X, Y);
    if e % 5000 == 0 {
        out(vcolors.yellow("Checkpoint: Epoch " + string(e)));
    }
};

vml.save_deep_model(crypto_net, "crypto_deep_v1");
out(vcolors.green("Deep Model trained and saved!"));