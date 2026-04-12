#===============================================================================
# VColors - Vyne Terminal Styling Engine
#===============================================================================

module vcolors;

# ----- Core ANSI Codes -----
const RESET   = "\033[0m";
const BOLD    = "\033[1m";
const ITALIC  = "\033[3m";
const UNDER   = "\033[4m";

# ----- Palette Group -----
group Palette {
    black   = "\033[0;30m";
    red     = "\033[0;31m";
    green   = "\033[0;32m";
    yellow  = "\033[0;33m";
    blue    = "\033[0;34m";
    magenta = "\033[0;35m";
    cyan    = "\033[0;36m";
    white   = "\033[0;37m";
};

# ----- Bright Palette Group -----
group Bright {
    black   = "\033[1;30m";
    red     = "\033[1;31m";
    green   = "\033[1;32m";
    yellow  = "\033[1;33m";
    blue    = "\033[1;34m";
    magenta = "\033[1;35m";
    cyan    = "\033[1;36m";
    white   = "\033[1;37m";
};

# ----- Background Group -----
group BG {
    red     = "\033[41m";
    green   = "\033[42m";
    yellow  = "\033[43m";
    blue    = "\033[44m";
    cyan    = "\033[46m";
};

# ============================================================================
# UTILITY WRAPPERS
# ============================================================================

fn :: vcolors paint(text, color) {
    return color + text + RESET;
}

# Standard Colors
fn :: vcolors red(t)     { return vcolors.paint(t, Palette.red); }
fn :: vcolors green(t)   { return vcolors.paint(t, Palette.green); }
fn :: vcolors yellow(t)  { return vcolors.paint(t, Palette.yellow); }
fn :: vcolors blue(t)    { return vcolors.paint(t, Palette.blue); }
fn :: vcolors cyan(t)    { return vcolors.paint(t, Palette.cyan); }
fn :: vcolors magenta(t) { return vcolors.paint(t, Palette.magenta); }
fn :: vcolors white(t)   { return vcolors.paint(t, Palette.white); } # Əlavə edildi

# Bold Variations
fn :: vcolors boldRed(t)    { return vcolors.paint(t, Bright.red); }
fn :: vcolors boldGreen(t)  { return vcolors.paint(t, Bright.green); }
fn :: vcolors boldCyan(t)   { return vcolors.paint(t, Bright.cyan); }
fn :: vcolors boldYellow(t) { return vcolors.paint(t, Bright.yellow); }
fn :: vcolors boldWhite(t)  { return vcolors.paint(t, Bright.white); } # XOR script üçün vacibdir

# Background Variations (Çatışmayan hissə bura idi)
fn :: vcolors bgRed(t)      { return vcolors.paint(t, BG.red); }
fn :: vcolors bgGreen(t)    { return vcolors.paint(t, BG.green); }
fn :: vcolors bgYellow(t)   { return vcolors.paint(t, BG.yellow); }
fn :: vcolors bgBlue(t)     { return vcolors.paint(t, BG.blue); }
fn :: vcolors bgCyan(t)     { return vcolors.paint(t, BG.cyan); }

# Styles
fn :: vcolors bold(t)      { return vcolors.paint(t, BOLD); }
fn :: vcolors italic(t)    { return vcolors.paint(t, ITALIC); }
fn :: vcolors underline(t) { return vcolors.paint(t, UNDER); }

# ============================================================================
# SEMANTIC SYSTEM
# ============================================================================

fn :: vcolors success(t) { return Palette.green + t + RESET; }
fn :: vcolors error(t)   { return Palette.red + t + RESET; }
fn :: vcolors warning(t) { return Palette.yellow + t + RESET; }
fn :: vcolors info(t)    { return Palette.blue + t + RESET; }

# ============================================================================
# ADVANCED STYLING
# ============================================================================

fn :: vcolors rainbow(text) {
    colors = [Palette.red, Palette.yellow, Palette.green, Palette.cyan, Palette.blue, Palette.magenta];
    result = "";
    i = 0;
    while i < text.length() {
        ch = text[i];
        colorIdx = i % 6;
        result = result + colors[colorIdx] + ch;
        i++;
    }
    return result + RESET;
}

fn :: vcolors box(text) {
    len = text.length();
    border = "";
    i = 0;
    while i < len + 4 {
        border = border + "─";
        i++;
    }
    res = Palette.cyan + "┌" + border + "┐\n";
    res = res + "│  " + text + "  │\n";
    res = res + "└" + border + "┘" + RESET;
    return res;
}

deploy vcolors;