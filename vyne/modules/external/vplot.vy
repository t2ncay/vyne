# Legacy text bar helper. The universal API is in vplot/vplot.vy.
ruleset { dynamic_casting };
module vplot;

fn :: vplot bar_chart(data :: Array, height :: Int64) {
    through i :: 0..data.size()-1 -> loop {
        line :: String := "";
        through j :: 1..height -> loop { line = line + "|"; };
        out(string(data[i]) + " " + line);
    };
}

deploy vplot;
