#===============================================================================
# vcolors_test.vy — smoke test for the VColors external module
# Copyright (c) 2026 Tuncay Gafarli — MIT License
#===============================================================================

ruleset { dynamic_casting, warnings, verbose };

use lib "vcolors.vy";

fn main() {
    out(vcolors.banner("VColors Smoke Test"));
    out(vcolors.rule());

    # --- Foreground ---
    out(vcolors.red("red"));
    out(vcolors.green("green"));
    out(vcolors.yellow("yellow"));
    out(vcolors.blue("blue"));
    out(vcolors.magenta("magenta"));
    out(vcolors.cyan("cyan"));
    out(vcolors.white("white"));
    out(vcolors.gray("gray"));
    out("");

    # --- Bold foreground ---
    out(vcolors.boldRed("bold red"));
    out(vcolors.boldGreen("bold green"));
    out(vcolors.boldCyan("bold cyan"));
    out(vcolors.boldWhite("bold white"));
    out("");

    # --- Background ---
    out(vcolors.onRed(" on red "));
    out(vcolors.onGreen(" on green "));
    out(vcolors.onYellow(" on yellow "));
    out(vcolors.onBlue(" on blue "));
    out("");

    # --- Styles ---
    out(vcolors.bold("bold"));
    out(vcolors.dim("dim"));
    out(vcolors.italic("italic"));
    out(vcolors.underline("underline"));
    out(vcolors.strikethrough("strikethrough"));
    out("");

    # --- Semantic ---
    out(vcolors.success("build succeeded"));
    out(vcolors.error("link failed"));
    out(vcolors.warning("deprecated API"));
    out(vcolors.info("using cached object"));
    out(vcolors.hint("pass --verbose for detail"));
    out("");

    # --- True color ---
    out(vcolors.rgb(255, 99, 71, "tomato"));
    out(vcolors.rgb(64, 224, 208, "turquoise"));
    out(vcolors.bgRgb(30, 30, 30, " dark panel "));
    out("");

    # --- Advanced ---
    out(vcolors.rainbow("The quick brown fox"));
    out(vcolors.rule());
    out(vcolors.box("Hello from Vyne!"));
    out(vcolors.rule());
}

main();