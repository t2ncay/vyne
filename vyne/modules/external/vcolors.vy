#===============================================================================
# VColors — Vyne Terminal Styling Engine
#-------------------------------------------------------------------------------
# MIT License
#
# Copyright (c) 2026 Tuncay Gafarli, Abdullah Novruzlu
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#===============================================================================

module vcolors;

# ============================================================================
# CORE ANSI CODES
# ============================================================================

const ESC     = "\e";
const RESET   = "\e[0m";
const BOLD    = "\e[1m";
const DIM     = "\e[2m";
const ITALIC  = "\e[3m";
const UNDER   = "\e[4m";
const BLINK   = "\e[5m";
const REVERSE = "\e[7m";
const HIDDEN  = "\e[8m";
const STRIKE  = "\e[9m";

# ============================================================================
# PALETTES
# ============================================================================

group Palette {
    black   = "\e[0;30m";
    red     = "\e[0;31m";
    green   = "\e[0;32m";
    yellow  = "\e[0;33m";
    blue    = "\e[0;34m";
    magenta = "\e[0;35m";
    cyan    = "\e[0;36m";
    white   = "\e[0;37m";
    gray    = "\e[0;90m";
};

group Bright {
    black   = "\e[1;30m";
    red     = "\e[1;31m";
    green   = "\e[1;32m";
    yellow  = "\e[1;33m";
    blue    = "\e[1;34m";
    magenta = "\e[1;35m";
    cyan    = "\e[1;36m";
    white   = "\e[1;37m";
};

group BG {
    black   = "\e[40m";
    red     = "\e[41m";
    green   = "\e[42m";
    yellow  = "\e[43m";
    blue    = "\e[44m";
    magenta = "\e[45m";
    cyan    = "\e[46m";
    white   = "\e[47m";
};

group BGBright {
    black   = "\e[100m";
    red     = "\e[101m";
    green   = "\e[102m";
    yellow  = "\e[103m";
    blue    = "\e[104m";
    magenta = "\e[105m";
    cyan    = "\e[106m";
    white   = "\e[107m";
};

# ============================================================================
# CORE WRAPPER
# ============================================================================

fn :: vcolors paint(text, color) {
    return color + text + RESET;
}

# ============================================================================
# FOREGROUND COLORS
# ============================================================================

fn :: vcolors black(t)   { return vcolors.paint(t, Palette.black);   }
fn :: vcolors red(t)     { return vcolors.paint(t, Palette.red);     }
fn :: vcolors green(t)   { return vcolors.paint(t, Palette.green);   }
fn :: vcolors yellow(t)  { return vcolors.paint(t, Palette.yellow);  }
fn :: vcolors blue(t)    { return vcolors.paint(t, Palette.blue);    }
fn :: vcolors magenta(t) { return vcolors.paint(t, Palette.magenta); }
fn :: vcolors cyan(t)    { return vcolors.paint(t, Palette.cyan);    }
fn :: vcolors white(t)   { return vcolors.paint(t, Palette.white);   }
fn :: vcolors gray(t)    { return vcolors.paint(t, Palette.gray);    }

# ============================================================================
# BOLD FOREGROUND
# ============================================================================

fn :: vcolors boldBlack(t)   { return vcolors.paint(t, Bright.black);   }
fn :: vcolors boldRed(t)     { return vcolors.paint(t, Bright.red);     }
fn :: vcolors boldGreen(t)   { return vcolors.paint(t, Bright.green);   }
fn :: vcolors boldYellow(t)  { return vcolors.paint(t, Bright.yellow);  }
fn :: vcolors boldBlue(t)    { return vcolors.paint(t, Bright.blue);    }
fn :: vcolors boldMagenta(t) { return vcolors.paint(t, Bright.magenta); }
fn :: vcolors boldCyan(t)    { return vcolors.paint(t, Bright.cyan);    }
fn :: vcolors boldWhite(t)   { return vcolors.paint(t, Bright.white);   }

# ============================================================================
# BACKGROUND
# ============================================================================

fn :: vcolors onBlack(t)   { return vcolors.paint(t, BG.black);   }
fn :: vcolors onRed(t)     { return vcolors.paint(t, BG.red);     }
fn :: vcolors onGreen(t)   { return vcolors.paint(t, BG.green);   }
fn :: vcolors onYellow(t)  { return vcolors.paint(t, BG.yellow);  }
fn :: vcolors onBlue(t)    { return vcolors.paint(t, BG.blue);    }
fn :: vcolors onMagenta(t) { return vcolors.paint(t, BG.magenta); }
fn :: vcolors onCyan(t)    { return vcolors.paint(t, BG.cyan);    }
fn :: vcolors onWhite(t)   { return vcolors.paint(t, BG.white);   }

# ============================================================================
# STYLES
# ============================================================================

fn :: vcolors bold(t)          { return vcolors.paint(t, BOLD);    }
fn :: vcolors dim(t)           { return vcolors.paint(t, DIM);     }
fn :: vcolors italic(t)        { return vcolors.paint(t, ITALIC);  }
fn :: vcolors underline(t)     { return vcolors.paint(t, UNDER);   }
fn :: vcolors blink(t)         { return vcolors.paint(t, BLINK);   }
fn :: vcolors reverse(t)       { return vcolors.paint(t, REVERSE); }
fn :: vcolors hidden(t)        { return vcolors.paint(t, HIDDEN);  }
fn :: vcolors strikethrough(t) { return vcolors.paint(t, STRIKE);  }

# ============================================================================
# SEMANTIC
# ============================================================================

fn :: vcolors success(t) { return Bright.green + "✔ " + t + RESET; }
fn :: vcolors error(t)   { return Bright.red   + "✘ " + t + RESET; }
fn :: vcolors warning(t) { return Bright.yellow + "⚠ " + t + RESET; }
fn :: vcolors info(t)    { return Bright.blue  + "ℹ " + t + RESET; }

fn :: vcolors hint(t)    { return Palette.gray + t + RESET; }

# ============================================================================
# 24-BIT TRUE COLOR
# ============================================================================

fn :: vcolors rgb(r, g, b, t) {
    return "\e[38;2;" + string(r) + ";" + string(g) + ";" + string(b) + "m" + t + RESET;
}

fn :: vcolors bgRgb(r, g, b, t) {
    return "\e[48;2;" + string(r) + ";" + string(g) + ";" + string(b) + "m" + t + RESET;
}

# ============================================================================
# ADVANCED
# ============================================================================

fn :: vcolors rainbow(text) {
    palette = [
        Palette.red, Palette.yellow, Palette.green,
        Palette.cyan, Palette.blue, Palette.magenta
    ];
    result = "";
    i = 0;
    len = text.length();
    while i < len {
        ch = text[i];
        colorIdx = i % 6;
        result = result + palette[colorIdx] + ch;
        i = i + 1;
    }
    return result + RESET;
}

fn :: vcolors box(text) {
    len = text.length();
    border = "";
    i = 0;
    while i < len + 4 {
        border = border + "-";
        i = i + 1;
    }
    top    = Palette.cyan + "+" + border + "+" + RESET + "\n";
    middle = Palette.cyan + "|" + RESET + "  " + text + "  " + Palette.cyan + "|" + RESET + "\n";
    bottom = Palette.cyan + "+" + border + "+" + RESET;
    return top + middle + bottom;
}

fn :: vcolors banner(text) {
    return Bright.cyan + BOLD + ">>> " + text + " <<<" + RESET;
}

fn :: vcolors rule() {
    return Palette.gray + "----------------------------------------" + RESET;
}

deploy vcolors;