ruleset { dynamic_casting };
module vglib;
module vnet;
module vcore;
module vmath;
module vfs;

# ====================================================================
# COLOR THEME CONFIGURATION
# ====================================================================
interface Theme {
    bg      :: Int64,
    panel   :: Int64,
    border  :: Int64,
    text    :: Int64,
    dim     :: Int64,
    accent  :: Int64
}

fn get_theme(name :: String) {
    if (name == "matrix") {
        return Theme(
            vglib.rgba(5, 15, 5, 255),
            vglib.rgba(10, 28, 10, 255),
            vglib.rgba(20, 80, 20, 255),
            vglib.rgba(50, 255, 50, 255),
            vglib.rgba(20, 140, 20, 255),
            vglib.rgba(0, 240, 255, 255)
        );
    }
    if (name == "cyberpunk") {
        return Theme(
            vglib.rgba(15, 5, 20, 255),
            vglib.rgba(30, 10, 38, 255),
            vglib.rgba(140, 20, 120, 255),
            vglib.rgba(255, 45, 120, 255),
            vglib.rgba(160, 30, 90, 255),
            vglib.rgba(0, 240, 255, 255)
        );
    }
    # Default Hacknet Amber/Cyan Palette
    return Theme(
        vglib.rgba(6, 10, 14, 255),
        vglib.rgba(14, 20, 28, 255),
        vglib.rgba(0, 240, 255, 100),
        vglib.rgba(0, 240, 255, 255),
        vglib.rgba(0, 160, 180, 255),
        vglib.rgba(255, 170, 0, 255)
    );
}

active_theme = get_theme("amber");

COLOR_GREEN = vglib.rgba(50, 255, 120, 255);
COLOR_RED   = vglib.rgba(255, 60, 60, 255);
COLOR_CYAN  = vglib.rgba(0, 240, 255, 255);
COLOR_AMBER = vglib.rgba(255, 170, 0, 255);

# ====================================================================
# NETWORK & ENGINE INITIALIZATION
# ====================================================================
my_port    :: Int64  = 8001; 
server_ip  :: String = "127.0.0.1";
server_port:: Int64  = 8000;

vglib.init(1280, 720, 60, "VYNE ADVANCED HACKNET TERMINAL - NODE: " + string(my_port), 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

client_sock = vnet.udp_socket(my_port);

# --- STATE VARIABLES ---
terminal_logs :: Array = [
    "[SYS_INIT]: VNET SOCKET BOUND TO PORT " + string(my_port),
    "[SYS_INIT]: MOUNTED KERNEL SUBSYSTEM (PID: " + string(vcore.pid) + ")",
    "TYPE 'help' FOR NETWORK & SYSTEM COMMANDS",
    "--------------------------------------------------"
];
input_buffer  :: String  = "";
server_status :: String  = "OFFLINE / UNKNOWN";
scroll_y      :: Float64 = 0.0;
cursor_blink  :: Float64 = 0.0;
run_time      :: Float64 = 0.0;

# ====================================================================
# STRING SANITIZER & PARSER
# ====================================================================
fn clean_str(raw :: String) -> String {
    out_str = raw;
    while (out_str.length() > 0) {
        c = out_str.substr(0, 1);
        if (c == " " || c == "\"" || c == "'" || c == "\t" || c == "\r" || c == "\n") {
            out_str = out_str.substr(1, out_str.length() - 1);
        } else {
            break;
        }
    }
    while (out_str.length() > 0) {
        idx = out_str.length() - 1;
        c   = out_str.substr(idx, 1);
        if (c == " " || c == "\"" || c == "'" || c == "\t" || c == "\r" || c == "\n") {
            out_str = out_str.substr(0, idx);
        } else {
            break;
        }
    }
    return out_str;
}

fn parse_input(raw :: String) -> Array {
    clean = raw;
    space_pos :: Int64 = -1;
    
    through i :: 0..(clean.length() - 1) -> loop {
        if (space_pos == -1 && clean[i] == " ") {
            space_pos = i;
        }
    };

    if (space_pos == -1) {
        return [clean, ""];
    }

    cmd_part = clean.substr(0, space_pos);
    arg_part = clean.substr(space_pos + 1, clean.length() - space_pos - 1);
    return [cmd_part, arg_part];
}

# ====================================================================
# SOURCE CODE ANALYZER
# ====================================================================
fn cmd_analyze(args :: String) -> Array {
    results :: Array = [];
    target = clean_str(args);

    if (target == "") {
        results.push("[ERROR]: Usage: analyze <filename.vy>");
        return results;
    }

    if (!vfs.exists(target)) {
        results.push("[ERROR]: Target file '" + target + "' not found.");
        results.push("  [CWD]: " + string(vfs.pwd()));
        return results;
    }

    source          :: String = vfs.read(target);
    file_bytes      :: Int64  = vfs.file_size(target);

    line_count       :: Int64 = 0;
    code_lines       :: Int64 = 0;
    comment_lines    :: Int64 = 0;
    blank_lines      :: Int64 = 0;
    fn_count         :: Int64 = 0;
    module_count     :: Int64 = 0;

    current_line :: String = "";

    through i :: 0..(source.length() - 1) -> loop {
        ch = source.substr(i, 1);

        if (ch == "\n" || i == (source.length() - 1)) {
            if (ch != "\n" && ch != "\r") { current_line = current_line + ch; }

            line_count++;
            trimmed = clean_str(current_line);

            if (trimmed.length() == 0) {
                blank_lines++;
            } else if (trimmed.substr(0, 1) == "#") {
                comment_lines++;
            } else {
                code_lines++;
                if (trimmed.substr(0, 3) == "fn ")     { fn_count++; }
                if (trimmed.substr(0, 7) == "module ") { module_count++; }
            }

            current_line = "";
        } else {
            if (ch != "\r") { current_line = current_line + ch; }
        }
    };

    results.push("==================================================");
    results.push(" [ANALYSIS REPORT]: " + target);
    results.push("--------------------------------------------------");
    results.push("  FILE SIZE:        " + string(file_bytes) + " bytes");
    results.push("  TOTAL LINES:      " + string(line_count));
    results.push("  EFFECTIVE LOC:    " + string(code_lines));
    results.push("  COMMENTS (#):     " + string(comment_lines));
    results.push("  FUNCTIONS (fn):   " + string(fn_count));
    results.push("  MODULE IMPORTS:   " + string(module_count));
    results.push("==================================================");
    return results;
}

# ====================================================================
# COMMAND DISPATCH ROUTER
# ====================================================================
fn dispatch_command(raw_input :: String) {
    clean_cmd = clean_str(raw_input);
    if (clean_cmd == "") { return null; }

    terminal_logs.push("> " + clean_cmd);

    parsed = parse_input(clean_cmd);
    cmd  = parsed[0];
    args = parsed[1];

    # --- HACKNET NETWORK COMMANDS ---
    if (cmd == "scan") {
        vnet.send_to(client_sock, server_ip, server_port, "SCAN:REQ");
    } 
    else if (cmd == "patch") {
        vnet.send_to(client_sock, server_ip, server_port, "PATCH:SEC_OVERRIDE");
    }
    else if (cmd == "crack") {
        if (args == "") {
            terminal_logs.push("[ERROR]: Usage: crack <value>");
        } else {
            vnet.send_to(client_sock, server_ip, server_port, "CRACK:" + args);
        }
    }
    else if (cmd == "cat") {
        if (args == "") {
            terminal_logs.push("[ERROR]: Usage: cat <vfs_path>");
        } else {
            vnet.send_to(client_sock, server_ip, server_port, "CAT:" + args);
        }
    }
    # --- LOCAL TERMINAL COMMANDS ---
    else if (cmd == "help") {
        terminal_logs.push("NETWORK COMMANDS:");
        terminal_logs.push("  scan             - Query target firewall & salt state");
        terminal_logs.push("  crack <val>      - Send hash attempt to breach firewall");
        terminal_logs.push("  cat <path>       - Read VFS file from remote target");
        terminal_logs.push("  patch            - [DEFENDER] Relock firewall & rotate salt");
        terminal_logs.push("LOCAL TERMINAL COMMANDS:");
        terminal_logs.push("  analyze <file>   - Static analysis on local .vy script");
        terminal_logs.push("  sysinfo          - Display host platform & hardware specs");
        terminal_logs.push("  memory           - Print resident physical RAM usage");
        terminal_logs.push("  uptime           - Show active session duration");
        terminal_logs.push("  theme <name>     - Switch theme (amber, matrix, cyberpunk)");
        terminal_logs.push("  clear            - Wipe terminal scroll buffer");
        terminal_logs.push("  exit             - Disconnect & close terminal");
    } 
    else if (cmd == "analyze") {
        lines = cmd_analyze(args);
        through line :: lines -> loop { terminal_logs.push(line); };
    }
    else if (cmd == "sysinfo") {
        terminal_logs.push("[SYS]: PLATFORM -> " + string(vcore.platform()));
        terminal_logs.push("[SYS]: ENGINE   -> " + string(vcore.engine));
        terminal_logs.push("[SYS]: CORES    -> " + string(vcore.processor_count));
        terminal_logs.push("[SYS]: PID      -> " + string(vcore.pid));
    } 
    else if (cmd == "memory") {
        mem_mb :: Float64 = vcore.memory_usage / (1024.0 * 1024.0);
        terminal_logs.push("[MEM]: WORKING SET: " + string(vmath.round(mem_mb * 100.0) / 100.0) + " MB");
    } 
    else if (cmd == "uptime") {
        sec :: Int64 = vmath.round(run_time);
        terminal_logs.push("[UPTIME]: Active for " + string(sec) + " seconds");
    } 
    else if (cmd == "theme") {
        if (args == "matrix" || args == "cyberpunk" || args == "amber") {
            active_theme = get_theme(args);
            terminal_logs.push("[THEME]: Applied '" + args + "' color scheme.");
        } else {
            terminal_logs.push("[ERROR]: Unknown theme. Available: amber, matrix, cyberpunk");
        }
    } 
    else if (cmd == "clear") {
        terminal_logs.clear();
        scroll_y = 0.0;
        terminal_logs = [
            "[SYS_INIT]: TERMINAL BUFFER CLEARED",
            "TYPE 'help' FOR COMMAND LIST",
            "--------------------------------------------------"
        ];
    } 
    else if (cmd == "exit") {
        vglib.close();
    } 
    else {
        terminal_logs.push("[ERR]: UNKNOWN COMMAND '" + cmd + "'");
    }

    # Auto scroll to bottom on new output
    if (terminal_logs.length() > 22) {
        scroll_y = float64(terminal_logs.length() - 22) * 24.0;
    }

    return null;
}

# ====================================================================
# MAIN ENGINE LOOP
# ====================================================================
while (vglib.running()) {
    run_time = run_time + 0.016;
    cursor_blink = cursor_blink + 0.016;

    # Scroll Wheel Control
    wheel = vglib.mouse_wheel();
    if (wheel != 0.0) {
        scroll_y = vmath.clamp(scroll_y - (wheel * 24.0), 0.0, 5000.0);
    }

    # Drain Char Input Stream (Fixes dropped keypresses)
    ch = vglib.get_char();
    while (ch != "") {
        input_buffer = input_buffer + ch;
        ch = vglib.get_char();
    }

    # Key Press Handlers
    if (vglib.key_pressed(vglib.ENTER)) {
        dispatch_command(input_buffer);
        input_buffer = "";
    }

    if (vglib.key_pressed(vglib.BACKSPACE)) {
        if (input_buffer.length() > 0) {
            input_buffer = input_buffer.substr(0, input_buffer.length() - 1);
        }
    }

    # Non-Blocking UDP Network Ingest
    packet_in :: Array = vnet.recv_from(client_sock);
    if (packet_in.length() >= 3) {
        net_msg :: String = string(packet_in[0]);
        terminal_logs.push("[NET_IN] " + net_msg);
        server_status = net_msg;

        if (terminal_logs.length() > 22) {
            scroll_y = float64(terminal_logs.length() - 22) * 24.0;
        }
    }

    # --- RENDER TERMINAL GUI ---
    vglib.begin();
        vglib.clear(active_theme.bg);

        # Header Status Bar
        vglib.rect(0, 0, 1280, 40, active_theme.panel);
        vglib.line(0, 40, 1280, 40, active_theme.border);
        vglib.text_ex(vcr_font, "VYNE ADVANCED TERMINAL | NODE: " + string(my_port), 16, 12, 14, active_theme.text);
        vglib.text_ex(vcr_font, "TARGET: " + server_ip + ":" + string(server_port), 920, 12, 12, active_theme.accent);

        # --- LEFT PANEL: LOG FEED & CLI TERMINAL ---
        vglib.rect(20, 60, 800, 640, active_theme.panel);
        vglib.line(20, 60, 820, 60, active_theme.border);

        log_count = terminal_logs.length();
        start_y :: Float64 = 75.0 - scroll_y;

        through i :: 0..(log_count - 1) -> loop {
            line_y :: Float64 = start_y + (i * 24.0);
            if (line_y >= 70.0 && line_y <= 630.0) {
                txt = string(terminal_logs[i]);
                col = active_theme.text;

                if (txt.substr(0, 2) == "> ")       { col = COLOR_GREEN; }
                if (txt.substr(0, 8) == "[NET_IN]") { col = COLOR_AMBER; }
                if (txt.substr(0, 7) == "[ERROR]" || txt.substr(0, 5) == "[ERR]") { col = COLOR_RED; }

                vglib.text_ex(vcr_font, txt, 35, line_y, 11, col);
            }
        };

        # Command Input Prompt Line
        vglib.line(20, 650, 820, 650, active_theme.border);
        prompt_text :: String = "CMD> " + input_buffer;
        vglib.text_ex(vcr_font, prompt_text, 30, 668, 12, COLOR_GREEN);

        # --- DYNAMICALLY MEASURED CURSOR (NO DRIFT) ---
        if (vmath.fmod(cursor_blink, 0.8) > 0.4) {
            text_size :: Array = vglib.measure_text(vcr_font, prompt_text, 12.0);
            cursor_x :: Float64 = 30.0 + float64(text_size[0]) + 2.0;
            vglib.rect(cursor_x, 668, 8, 14, active_theme.text);
        }

        # --- RIGHT PANEL: TELEMETRY & VFS ---
        vglib.rect(840, 60, 420, 640, active_theme.panel);
        vglib.line(840, 60, 1260, 60, active_theme.border);
        
        vglib.text_ex(vcr_font, "SYSTEM TELEMETRY & VFS", 855, 75, 12, active_theme.text);
        vglib.line(855, 95, 1245, 95, active_theme.border);

        vglib.text_ex(vcr_font, "HOST PLATFORM: " + string(vcore.platform()), 855, 110, 10, active_theme.dim);
        vglib.text_ex(vcr_font, "HOST PID:      " + string(vcore.pid), 855, 128, 10, active_theme.dim);

        mem_mb :: Float64 = vcore.memory_usage / (1024.0 * 1024.0);
        vglib.text_ex(vcr_font, "HOST RAM: " + string(vmath.round(mem_mb)) + " MB", 855, 150, 10, active_theme.text);
        vglib.rect(855, 168, 380, 10, vglib.rgba(20, 28, 38, 255));
        
        bar_fill :: Float64 = vmath.clamp((mem_mb / 512.0) * 380.0, 10.0, 380.0);
        vglib.rect(855, 168, bar_fill, 10, active_theme.text);

        vglib.line(855, 195, 1245, 195, active_theme.border);

        vglib.text_ex(vcr_font, "LAST NET RESPONSE:", 855, 210, 11, COLOR_CYAN);
        vglib.text_ex(vcr_font, server_status, 855, 230, 10, COLOR_GREEN);

        vglib.text_ex(vcr_font, "KNOWN VFS TARGET PATHS:", 855, 280, 11, COLOR_CYAN);
        vglib.text_ex(vcr_font, "1. /sys/firewall.cfg", 855, 305, 10, COLOR_AMBER);
        vglib.text_ex(vcr_font, "2. /sys/logs.txt", 855, 325, 10, COLOR_AMBER);
        vglib.text_ex(vcr_font, "3. /vault/data.key", 855, 345, 10, COLOR_RED);

        vglib.text_ex(vcr_font, "HASH ALGORITHM FORMULA:", 855, 390, 11, COLOR_CYAN);
        vglib.text_ex(vcr_font, "H = (CRACK_VAL * SALT) % 9999", 855, 415, 10, COLOR_GREEN);

    vglib.end();
}

vnet.close(client_sock);
vglib.close();