# vcolors.vy - Terminal colors and styling module
module vcolors;

# ----- Reset -----
const RESET = "\033[0m";

# ----- Regular Colors -----
const BLACK = "\033[0;30m";
const RED = "\033[0;31m";
const GREEN = "\033[0;32m";
const YELLOW = "\033[0;33m";
const BLUE = "\033[0;34m";
const MAGENTA = "\033[0;35m";
const CYAN = "\033[0;36m";
const WHITE = "\033[0;37m";

# ----- Bold/Bright Colors -----
const BOLD_BLACK = "\033[1;30m";
const BOLD_RED = "\033[1;31m";
const BOLD_GREEN = "\033[1;32m";
const BOLD_YELLOW = "\033[1;33m";
const BOLD_BLUE = "\033[1;34m";
const BOLD_MAGENTA = "\033[1;35m";
const BOLD_CYAN = "\033[1;36m";
const BOLD_WHITE = "\033[1;37m";

# ----- Background Colors -----
const BG_BLACK = "\033[40m";
const BG_RED = "\033[41m";
const BG_GREEN = "\033[42m";
const BG_YELLOW = "\033[43m";
const BG_BLUE = "\033[44m";
const BG_MAGENTA = "\033[45m";
const BG_CYAN = "\033[46m";
const BG_WHITE = "\033[47m";

# ----- Styles -----
const BOLD = "\033[1m";
const DIM = "\033[2m";
const ITALIC = "\033[3m";
const UNDERLINE = "\033[4m";
const BLINK = "\033[5m";
const REVERSE = "\033[7m";
const HIDDEN = "\033[8m";
const STRIKE = "\033[9m";

# ============================================================================
# BASIC COLOR FUNCTIONS
# ============================================================================

fn :: vcolors black(text) {
    return BLACK + text + RESET;
}

fn :: vcolors red(text) {
    return RED + text + RESET;
}

fn :: vcolors green(text) {
    return GREEN + text + RESET;
}

fn :: vcolors yellow(text) {
    return YELLOW + text + RESET;
}

fn :: vcolors blue(text) {
    return BLUE + text + RESET;
}

fn :: vcolors magenta(text) {
    return MAGENTA + text + RESET;
}

fn :: vcolors cyan(text) {
    return CYAN + text + RESET;
}

fn :: vcolors white(text) {
    return WHITE + text + RESET;
}

# ============================================================================
# BOLD COLOR FUNCTIONS
# ============================================================================

fn :: vcolors boldBlack(text) {
    return BOLD_BLACK + text + RESET;
}

fn :: vcolors boldRed(text) {
    return BOLD_RED + text + RESET;
}

fn :: vcolors boldGreen(text) {
    return BOLD_GREEN + text + RESET;
}

fn :: vcolors boldYellow(text) {
    return BOLD_YELLOW + text + RESET;
}

fn :: vcolors boldBlue(text) {
    return BOLD_BLUE + text + RESET;
}

fn :: vcolors boldMagenta(text) {
    return BOLD_MAGENTA + text + RESET;
}

fn :: vcolors boldCyan(text) {
    return BOLD_CYAN + text + RESET;
}

fn :: vcolors boldWhite(text) {
    return BOLD_WHITE + text + RESET;
}

# ============================================================================
# BACKGROUND COLOR FUNCTIONS
# ============================================================================

fn :: vcolors bgBlack(text) {
    return BG_BLACK + text + RESET;
}

fn :: vcolors bgRed(text) {
    return BG_RED + text + RESET;
}

fn :: vcolors bgGreen(text) {
    return BG_GREEN + text + RESET;
}

fn :: vcolors bgYellow(text) {
    return BG_YELLOW + text + RESET;
}

fn :: vcolors bgBlue(text) {
    return BG_BLUE + text + RESET;
}

fn :: vcolors bgMagenta(text) {
    return BG_MAGENTA + text + RESET;
}

fn :: vcolors bgCyan(text) {
    return BG_CYAN + text + RESET;
}

fn :: vcolors bgWhite(text) {
    return BG_WHITE + text + RESET;
}

# ============================================================================
# TEXT STYLE FUNCTIONS
# ============================================================================

fn :: vcolors bold(text) {
    return BOLD + text + RESET;
}

fn :: vcolors dim(text) {
    return DIM + text + RESET;
}

fn :: vcolors italic(text) {
    return ITALIC + text + RESET;
}

fn :: vcolors underline(text) {
    return UNDERLINE + text + RESET;
}

fn :: vcolors blink(text) {
    return BLINK + text + RESET;
}

fn :: vcolors reverse(text) {
    return REVERSE + text + RESET;
}

fn :: vcolors hidden(text) {
    return HIDDEN + text + RESET;
}

fn :: vcolors strike(text) {
    return STRIKE + text + RESET;
}

# ============================================================================
# COMBINED STYLE FUNCTIONS
# ============================================================================

fn :: vcolors boldRed(text) {
    return BOLD_RED + text + RESET;
}

fn :: vcolors boldGreen(text) {
    return BOLD_GREEN + text + RESET;
}

fn :: vcolors underlineBlue(text) {
    return UNDERLINE + BLUE + text + RESET;
}

fn :: vcolors italicYellow(text) {
    return ITALIC + YELLOW + text + RESET;
}

fn :: vcolors bgRedBoldWhite(text) {
    return BG_RED + BOLD_WHITE + text + RESET;
}

# ============================================================================
# SEMANTIC FUNCTIONS
# ============================================================================

fn :: vcolors success(text) {
    return GREEN + "✓ " + text + RESET;
}

fn :: vcolors error(text) {
    return RED + "✗ " + text + RESET;
}

fn :: vcolors warning(text) {
    return YELLOW + "⚠ " + text + RESET;
}

fn :: vcolors info(text) {
    return BLUE + "ℹ " + text + RESET;
}

fn :: vcolors debug(text) {
    return MAGENTA + "🔍 " + text + RESET;
}

fn :: vcolors highlight(text) {
    return BG_YELLOW + BOLD + text + RESET;
}

# ============================================================================
# DIRECT PRINT FUNCTIONS
# ============================================================================

fn :: vcolors printRed(text) {
    out(red(text));
}

fn :: vcolors printGreen(text) {
    out(green(text));
}

fn :: vcolors printYellow(text) {
    out(yellow(text));
}

fn :: vcolors printBlue(text) {
    out(blue(text));
}

fn :: vcolors printSuccess(text) {
    out(success(text));
}

fn :: vcolors printError(text) {
    out(error(text));
}

fn :: vcolors printWarning(text) {
    out(warning(text));
}

fn :: vcolors printInfo(text) {
    out(info(text));
}

fn :: vcolors printDebug(text) {
    out(debug(text));
}

# ============================================================================
# FUN EXTRAS
# ============================================================================

fn :: vcolors rainbow(text) {
    colors = [RED, YELLOW, GREEN, CYAN, BLUE, MAGENTA];
    result = "";
    i = 0;
    
    while i < sizeof(text) {
        ch = text[i];
        colorIdx = i % 6;
        result = result + colors[colorIdx] + ch;
        i = i + 1;
    }
    
    return result + RESET;
}

# TODO: MAKE ACCESING STRING'S ELEMENT POSSIBLE

fn :: vcolors box(text) {
    len = 32;
    line = "";
    i = 0;
    while i < len + 4 {
        line = line + "─";
        i = i + 1;
    }
    
    result = "┌" + line + "┐\n";
    result = result + "│  " + text + "  │\n";
    result = result + "└" + line + "┘";
    
    return CYAN + result + RESET;
}

deploy vcolors;