ruleset { dynamic_casting };
module vglib;
module vcore;
module vmath;

# --- INITIALIZE TUI ENGINE & DISPLAY ---
vglib.init(1280, 800, 60, "VYNE ADVANCED SYSTEM TERMINAL v2.4", 0);
vcr_font = vglib.load_font("fonts/vcr_mono.ttf");

# --- COLOR PALETTE (AMBER CRT THEME) ---
COLOR_BG        = vglib.rgba(12, 10, 8, 255);
COLOR_PANEL     = vglib.rgba(22, 18, 14, 255);
COLOR_BORDER    = vglib.rgba(80, 60, 30, 255);
COLOR_TEXT      = vglib.rgba(255, 170, 0, 255);     # CRT Amber
COLOR_TEXT_DIM  = vglib.rgba(160, 110, 20, 255);
COLOR_GREEN     = vglib.rgba(50, 255, 120, 255);
COLOR_RED       = vglib.rgba(255, 60, 60, 255);
COLOR_CYAN      = vglib.rgba(0, 240, 255, 255);

# --- TERMINAL STATE VARIABLES ---
command_input  :: String  = "";
cursor_blink   :: Float64 = 0.0;
scroll_y       :: Float64 = 0.0;
donut_angle_a  :: Float64 = 0.0;
donut_angle_b  :: Float64 = 0.0;
run_time       :: Float64 = 0.0;

# Output Buffer for Command Execution & History
history_logs :: Array = [
    "[SYSTEM]: VYNE OS KERNEL INITIALIZED",
    "[SYSTEM]: MOUNTING VCORE MODULE (PID: " + string(vcore.pid) + ")",
    "[SYSTEM]: GRAPHICS ENGINE READY - VGLIB v0.0.4-alpha",
    "Type 'help' to see available terminal commands.",
    "--------------------------------------------------"
];

# --- COMMAND PROCESSOR ---
fn execute_command(cmd) {
    clean_cmd = cmd;
    
    # Append user prompt to log history
    history_logs.push("> " + clean_cmd);

    if (clean_cmd == "help") {
        history_logs.push("AVAILABLE COMMANDS:");
        history_logs.push("  help      - Display this system manual");
        history_logs.push("  sysinfo   - Dump detailed hardware/platform specs");
        history_logs.push("  memory    - Print current resident physical RAM usage");
        history_logs.push("  clear     - Wipe output buffer screen");
        history_logs.push("  exit      - Terminate terminal instance");
    } else if (clean_cmd == "sysinfo") {
        history_logs.push("[SYSINFO]: ENGINE: " + string(vcore.engine));
        history_logs.push("[SYSINFO]: PLATFORM: " + string(vcore.platform()));
        history_logs.push("[SYSINFO]: BUILD STAMP: " + string(vcore.build));
        history_logs.push("[SYSINFO]: CPU CORES: " + string(vcore.processor_count));
    } else if (clean_cmd == "memory") {
        mem_mb :: Float64 = vcore.memory_usage / (1024.0 * 1024.0);
        history_logs.push("[MEM]: WORKING SET RAM: " + string(vmath.round(mem_mb * 100.0) / 100.0) + " MB");
    } else if (clean_cmd == "clear") {
        history_logs = [];
    } else if (clean_cmd == "exit") {
        vglib.close();
    } else if (clean_cmd != "") {
        history_logs.push("[ERROR]: Unrecognized command '" + clean_cmd + "'");
    }
}

# --- MAIN ENGINE LOOP ---
while (vglib.running()) {
    run_time = run_time + 0.016;
    cursor_blink = cursor_blink + 0.016;

    # Advance 3D Donut Rotation Angles
    donut_angle_a = donut_angle_a + 0.04;
    donut_angle_b = donut_angle_b + 0.02;

    # Handle Mouse Wheel Scrolling for Terminal Output
    wheel = vglib.mouse_wheel();
    if (wheel != 0.0) {
        scroll_y = vmath.clamp(scroll_y - (wheel * 20.0), 0.0, 2000.0);
    }

    # ====================================================================
    # DYNAMIC KEYBOARD INPUT PROCESSOR
    # ====================================================================
    # Poll printable characters (Letters A-Z, Numbers 0-9, Symbols, Space)
    char_code = vglib.get_char();
    while (char_code > 0) {
        if (char_code >= 32 && char_code <= 125) {
            command_input = command_input + string(char_code);
        }
        char_code = vglib.get_char();
    }

    # Control Keys Handling
    if (vglib.key_pressed(vglib.ENTER)) {
        execute_command(command_input);
        command_input = "";
    }

    if (vglib.key_pressed(vglib.BACKSPACE)) {
        if (command_input.length() > 0) {
            command_input = command_input.substr(0, command_input.length() - 1);
        }
    }

    # Render Terminal Window Frame
    vglib.begin();
        vglib.clear(COLOR_BG);

        # ====================================================================
        # TOP HEADER STATUS BAR (Y: 0..40)
        # ====================================================================
        vglib.rect(0, 0, 1280, 40, COLOR_PANEL);
        vglib.line(0, 40, 1280, 40, COLOR_BORDER);

        vglib.text_ex(vcr_font, "VYNE SYSTEM TERMINAL v2.4", 16, 12, 14, COLOR_TEXT);
        vglib.text_ex(vcr_font, "TIME: " + vcore.now(), 450, 12, 12, COLOR_TEXT_DIM);
        
        fps_str = "FPS: " + string(vglib.get_fps());
        vglib.text_ex(vcr_font, fps_str, 1180, 12, 12, COLOR_GREEN);

        # ====================================================================
        # MAIN TERMINAL BUFFER PANEL (LEFT - X: 20..840)
        # ====================================================================
        vglib.rect(20, 60, 820, 660, COLOR_PANEL);
        vglib.line(20, 60, 840, 60, COLOR_BORDER);
        vglib.line(840, 60, 840, 720, COLOR_BORDER);
        vglib.line(840, 720, 20, 720, COLOR_BORDER);
        vglib.line(20, 720, 20, 60, COLOR_BORDER);

        # Render Log History Buffer Lines
        log_count = history_logs.length();
        start_y :: Float64 = 75.0 - scroll_y;

        through i :: 0..(log_count - 1) -> loop {
            line_y :: Float64 = start_y + (i * 22.0);
            if (line_y >= 70.0 && line_y <= 660.0) {
                txt = history_logs[i];
                col = COLOR_TEXT;
                
                # Dynamic syntax highlighting for logs
                if (txt.substr(0, 8) == "[SYSTEM]") { col = COLOR_CYAN; }
                if (txt.substr(0, 7) == "[ERROR]")  { col = COLOR_RED; }
                if (txt.substr(0, 2) == "> ")       { col = COLOR_GREEN; }

                vglib.text_ex(vcr_font, txt, 35, line_y, 11, col);
            }
        };

        # Input Prompt Line (Fixed at Bottom of Panel)
        vglib.line(20, 675, 840, 675, COLOR_BORDER);
        vglib.text_ex(vcr_font, "vyne@root:~$ " + command_input, 35, 690, 12, COLOR_TEXT);

        # Blinking CRT Cursor
        if (vmath.fmod(cursor_blink, 0.8) > 0.4) {
            cursor_x :: Float64 = 175.0 + (command_input.length() * 9.5);
            vglib.rect(cursor_x, 690, 8, 14, COLOR_TEXT);
        }

        # ====================================================================
        # TELEMETRY & 3D ASCII DONUT PANEL (RIGHT - X: 870..1260)
        # ====================================================================
        vglib.rect(870, 60, 390, 660, COLOR_PANEL);
        vglib.line(870, 60, 1260, 60, COLOR_BORDER);
        vglib.line(1260, 60, 1260, 720, COLOR_BORDER);
        vglib.line(1260, 720, 870, 720, COLOR_BORDER);
        vglib.line(870, 720, 870, 60, COLOR_BORDER);

        vglib.text_ex(vcr_font, "SYSTEM TELEMETRY", 890, 80, 12, COLOR_TEXT);
        vglib.line(890, 100, 1240, 100, COLOR_BORDER);

        # Hardware Info Readouts
        vglib.text_ex(vcr_font, "HOST OS:  " + string(vcore.platform()), 890, 115, 10, COLOR_TEXT_DIM);
        vglib.text_ex(vcr_font, "PID:      " + string(vcore.pid), 890, 135, 10, COLOR_TEXT_DIM);
        vglib.text_ex(vcr_font, "CORES:    " + string(vcore.processor_count), 890, 155, 10, COLOR_TEXT_DIM);

        # Memory Progress Gauge (Inlined memory calculation)
        mem_mb :: Float64 = vcore.memory_usage / (1024.0 * 1024.0);
        vglib.text_ex(vcr_font, "RAM: " + string(vmath.round(mem_mb)) + " MB", 890, 185, 10, COLOR_TEXT);
        vglib.rect(890, 205, 350, 12, vglib.rgba(35, 28, 20, 255));
        
        bar_fill :: Float64 = vmath.clamp((mem_mb / 512.0) * 350.0, 10.0, 350.0);
        vglib.rect(890, 205, bar_fill, 12, COLOR_TEXT);

        # CRT Scanlines / Ambient Grid Panel
        vglib.line(890, 240, 1240, 240, COLOR_BORDER);
        vglib.text_ex(vcr_font, "MATH CO-PROCESSOR [DONUT.C]", 890, 255, 11, COLOR_CYAN);

        # Render Live ASCII Math Donut onto Canvas
        vglib.donut(donut_angle_a, donut_angle_b);

        # ====================================================================
        # BOTTOM FOOTER STATUS STRIP (Y: 740..800)
        # ====================================================================
        vglib.rect(0, 740, 1280, 60, vglib.rgba(16, 12, 10, 255));
        vglib.line(0, 740, 1280, 740, COLOR_BORDER);
        vglib.text_ex(vcr_font, "STATUS: ONLINE | INPUT: UTF8-ACTIVE | SCROLL: " + string(vmath.round(scroll_y)), 20, 762, 10, COLOR_TEXT_DIM);

    vglib.end();
}

# Cleanup and terminate subsystems
vglib.close();