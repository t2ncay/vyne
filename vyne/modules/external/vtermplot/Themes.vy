# vtermplot/Themes.vy - glyph tables for Unicode and ASCII rendering.

ruleset { dynamic_casting };

module vtermplot;

const HEAT_ASCII :: Array :=
    [" ", ".", ":", "-", "=", "+", "*", "#", "%", "@"];
const HEAT_UNI :: Array :=
    [" ", "░", "░", "▒", "▒", "▓", "▓", "█", "█", "█"];
const SPARK_ASCII :: Array := ["_", ".", "-", "=", "+", "*", "#", "@"];
const SPARK_UNI :: Array := ["▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"];

fn :: vtermplot heat_char(level :: Int64, unicode :: Bool) -> String {
    idx :: Int64 := level;
    if idx < 0 { idx = 0; }
    if idx > 9 { idx = 9; }
    if unicode { return HEAT_UNI[idx]; }
    return HEAT_ASCII[idx];
}

fn :: vtermplot spark_char(level :: Int64, unicode :: Bool) -> String {
    idx :: Int64 := level;
    if idx < 0 { idx = 0; }
    if idx > 7 { idx = 7; }
    if unicode { return SPARK_UNI[idx]; }
    return SPARK_ASCII[idx];
}

fn :: vtermplot point_char(unicode :: Bool) -> String {
    if unicode { return "•"; }
    return "*";
}

fn :: vtermplot line_char(unicode :: Bool) -> String {
    if unicode { return "•"; }
    return "*";
}

fn :: vtermplot bar_char(unicode :: Bool) -> String {
    if unicode { return "█"; }
    return "#";
}

fn :: vtermplot hline_char(unicode :: Bool) -> String {
    if unicode { return "─"; }
    return "-";
}

fn :: vtermplot vline_char(unicode :: Bool) -> String {
    if unicode { return "│"; }
    return "|";
}
