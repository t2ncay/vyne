ruleset { dynamic_casting };
module vglib;
module vnet;
module vcore;
module vmath;
module vfs;

# ====================================================================
# NETWORK CONFIGURATION & SOCKET SETUP
# ====================================================================
my_port     :: Int64  = int64(vmath.random(8001, 8999));
server_ip   :: String = "127.0.0.1";
server_port :: Int64  = 8000;

vglib.init(1280, 800, 60, "VYNE SHADOWOS v7.0 - DEEP VNET & HACKNET TERMINAL", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

client_sock = vnet.udp_socket(my_port);

# ====================================================================
# COLOR PALETTE & THEME SYSTEM
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
    # Pitch Black Default
    return Theme(
        vglib.rgba(2, 2, 4, 255),
        vglib.rgba(10, 12, 16, 255),
        vglib.rgba(40, 50, 60, 255),
        vglib.rgba(0, 220, 240, 255),
        vglib.rgba(160, 170, 185, 255),
        vglib.rgba(255, 150, 0, 255)
    );
}

active_theme = get_theme("amber");

COLOR_BLACK      = vglib.rgba(2, 2, 4, 255);
COLOR_PANEL      = vglib.rgba(10, 12, 16, 255);
COLOR_CLI_BG     = vglib.rgba(6, 8, 12, 245);       # Semi-transparent Terminal Panel
COLOR_URLBAR     = vglib.rgba(16, 20, 26, 255);
COLOR_BORDER     = vglib.rgba(40, 50, 60, 255);
COLOR_BLOOD      = vglib.rgba(220, 20, 40, 255);
COLOR_CYAN       = vglib.rgba(0, 220, 240, 255);
COLOR_AMBER      = vglib.rgba(255, 150, 0, 255);
COLOR_TOXIC      = vglib.rgba(40, 240, 100, 255);
COLOR_GHOST      = vglib.rgba(160, 170, 185, 255);
COLOR_SCANLINE   = vglib.rgba(0, 0, 0, 90);

# ====================================================================
# SYSTEM & GAME STATE VARIABLES
# ====================================================================
current_url      :: String  = "shadow.dir";
input_url        :: String  = "shadow.dir";
url_focused      :: Int64   = 0;

cli_overlay_open :: Int64   = 0;                     # Toggleable CLI Overlay State
cli_input_buffer :: String  = "";
cli_logs         :: Array   = [
    "[SYS_INIT]: VNET SOCKET BOUND TO PORT " + string(my_port),
    "[SYS_INIT]: MOUNTED KERNEL SUBSYSTEM (PID: " + string(vcore.pid) + ")",
    "PRESS [TAB] TO TOGGLE OVERLAY TERMINAL ANYTIME",
    "TYPE 'help' FOR NETWORK & SYSTEM COMMANDS",
    "--------------------------------------------------"
];
server_status    :: String  = "OFFLINE / UNKNOWN";

trace_level      :: Int64   = 14;
scroll_y         :: Float64 = 0.0;
cli_scroll_y     :: Float64 = 0.0;
glitch_trigger   :: Float64 = 0.0;
run_time         :: Float64 = 0.0;
cursor_blink     :: Float64 = 0.0;
mouse_was_down   :: Int64   = 0;

page_body        :: Array   = [];

# ====================================================================
# STRING SANITIZER & PARSER HELPERS
# ====================================================================
fn clean_str(raw :: String) -> String {
    out_str = raw;
    while (out_str.length() > 0) {
        c = out_str.substr(0, 1);
        if (c == " " || c == "\"" || c == "'" || c == "\t" || c == "\r" || c == "\n") {
            out_str = out_str.substr(1, out_str.length() - 1);
        } else { break; }
    }
    while (out_str.length() > 0) {
        idx = out_str.length() - 1;
        c   = out_str.substr(idx, 1);
        if (c == " " || c == "\"" || c == "'" || c == "\t" || c == "\r" || c == "\n") {
            out_str = out_str.substr(0, idx);
        } else { break; }
    }
    return out_str;
}

fn parse_input(raw :: String) -> Array {
    clean = raw;
    space_pos :: Int64 = -1;
    through i :: 0..(clean.length() - 1) -> loop {
        if (space_pos == -1 && clean[i] == " ") { space_pos = i; }
    };
    if (space_pos == -1) { return [clean, ""]; }
    cmd_part = clean.substr(0, space_pos);
    arg_part = clean.substr(space_pos + 1, clean.length() - space_pos - 1);
    return [cmd_part, arg_part];
}

# ====================================================================
# SOURCE SCRIPT ANALYZER
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
        return results;
    }

    source     :: String = vfs.read(target);
    file_bytes :: Int64  = vfs.file_size(target);
    line_count :: Int64  = 0;
    code_lines :: Int64  = 0;
    current_line :: String = "";

    through i :: 0..(source.length() - 1) -> loop {
        ch = source.substr(i, 1);
        if (ch == "\n" || i == (source.length() - 1)) {
            if (ch != "\n" && ch != "\r") { current_line = current_line + ch; }
            line_count++;
            trimmed = clean_str(current_line);
            if (trimmed.length() > 0 && trimmed.substr(0, 1) != "#") { code_lines++; }
            current_line = "";
        } else {
            if (ch != "\r") { current_line = current_line + ch; }
        }
    };

    results.push("==================================================");
    results.push(" [ANALYSIS REPORT]: " + target + " (" + string(file_bytes) + " bytes)");
    results.push("  TOTAL LINES: " + string(line_count) + " | EFFECTIVE LOC: " + string(code_lines));
    results.push("==================================================");
    return results;
}

# ====================================================================
# 8-SITE DEEP VNET DATABASE
# ====================================================================
fn load_page(url :: String) -> Array {
    clean_u = url;
    
    if (clean_u == "shadow.dir") {
        return [
            "[TITLE] SHADOWNET ANONYMOUS DIRECTORY v4.09",
            "[HR]",
            "[WARN] NOTICE: ALL ROUTE TRAFFIC UNENCRYPTED BEYOND THIS NODE.",
            "[TEXT] Welcome to the ShadowNet Onion Index. Select a destination below.",
            "[TEXT] Keep your CLI overlay active. Hostile network tracers scan this directory.",
            "[HR]",
            "[SUBTITLE] CATEGORY: MARKETPLACES & DATA EXCHANGES",
            "[LINK:market.vnet] -> The Red Market (Exploits & Stolen Database Keys)",
            "[LINK:crypto.vnet] -> Black Tumbler Wallet Ledger & Crypto Mixer",
            "[TEXT] ",
            "[SUBTITLE] CATEGORY: RESTRICTED STREAMS & SURVEILLANCE",
            "[LINK:dollhouse.vnet] -> Live Surveillance Feed #0992 (Dollhouse)",
            "[LINK:redroom.vnet] -> Encrypted Stream Node Alpha (Restricted Feed)",
            "[TEXT] ",
            "[SUBTITLE] CATEGORY: DATA VAULTS & FORUMS",
            "[LINK:vault.vnet] -> Corrupted VFS Memory Dump /vault/sys/",
            "[LINK:forum.vnet] -> /b/ - Anonymous Unfiltered Message Board",
            "[LINK:terminal.vnet] -> Master Decryption Gateway Node",
            "[HR]",
            "[CODE] NODE DISCLAIMER: ROUTE_HASH_KEY_1 [9482] - DO NOT SHARE THIS KEY.",
            "[TEXT] End of index directory."
        ];
    }
    if (clean_u == "market.vnet") {
        return [
            "[TITLE] THE RED MARKET - BLACK DATA EXCHANGE",
            "[HR]",
            "[WARN] WARNING: HIGH THREAT NODE. LOCAL IP EXPOSED TO SWARM.",
            "[TEXT] Stolen corporate databases, root credentials, and zero-day payloads.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | ITEM #881: MILITARY FIRMWARE DUMP                         |",
            "[BOX] | DETAILS: KEY_FRAGMENT_2 IS [1337] FOR DECRYPTION ENGINE   |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] ",
            "[LINK:vault.vnet] >> JUMP TO CORRUPTED DATA VAULT",
            "[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY",
            "[HR]"
        ];
    }
    if (clean_u == "dollhouse.vnet") {
        return [
            "[TITLE] SURVEILLANCE FEED #0992 - ROOM 402",
            "[HR]",
            "[WARN] ACTIVE FEED CONNECTED. REVERSE TRACE DETECTED ON PORT 8001.",
            "[BOX] +---------------------------------------------------------+",
            "[BOX] | CAMERA: CAM_402_NORTH | KEY_3: [8008]                   |",
            "[BOX] +---------------------------------------------------------+",
            "[TEXT] [AUDIO SENSOR]: Heavy footsteps echoing in eastern hallway...",
            "[LINK:shadow.dir] << DISCONNECT AND EXIT IMMEDIATELY",
            "[HR]"
        ];
    }
    if (clean_u == "vault.vnet") {
        return [
            "[TITLE] CORRUPTED DATA VAULT /VFS/MEMORY/",
            "[HR]",
            "[CODE] 00000000: 4F 70 65 6E 53 53 4C 20 4B 65 79 20 44 75 6D 70",
            "[CODE] 00000010: 4B 45 59 5F 34 3A 20 5B 34 30 34 30 5D 20 4F 4B",
            "[HR]",
            "[LINK:market.vnet] >> RETURN TO RED MARKET",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "forum.vnet") {
        return [
            "[TITLE] /B/ - ANONYMOUS UNFILTERED MESSAGE BOARD",
            "[HR]",
            "[TEXT] Anonymous_991: Has anyone gathered all 8 key codes yet?",
            "[TEXT] Ghost_User: I found key 5 buried in this post signature [7712].",
            "[HR]",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY"
        ];
    }
    if (clean_u == "redroom.vnet") {
        return [
            "[TITLE] STREAM NODE ALPHA [RESTRICTED ACCESS]",
            "[HR]",
            "[WARN] HIGH SECURITY ALERT: TRANSMISSION MONITORED BY HOSTILE TRACER.",
            "[BOX] | SIGNAL STATUS: ENCRYPTED | STREAM HASH: [6660]           |",
            "[LINK:shadow.dir] << TERMINATE STREAM CONNECTION",
            "[HR]"
        ];
    }
    if (clean_u == "crypto.vnet") {
        return [
            "[TITLE] BLACK TUMBLER WALLET LEDGER",
            "[HR]",
            "[CODE] TX_ID #9081 | 12.4 BTC | CONFIRMED | KEY_7_HASH: [3141]",
            "[LINK:shadow.dir] << RETURN TO DIRECTORY",
            "[HR]"
        ];
    }
    if (clean_u == "terminal.vnet") {
        return [
            "[TITLE] MASTER DECRYPTION GATEWAY",
            "[HR]",
            "[WARN] FINAL NODE REACHED. INPUT ALL CODES INTO OVERLAY CLI.",
            "[TEXT] Terminal Register Code 8: [9999]",
            "[LINK:shadow.dir] << RETURN TO MAIN DIRECTORY"
        ];
    }

    return [
        "[TITLE] 404 - NODE UNREACHABLE",
        "[HR]",
        "[WARN] COULD NOT RESOLVE VNET DOMAIN OVER TOR ROUTER.",
        "[LINK:shadow.dir] << RETURN TO SHADOWNET DIRECTORY"
    ];
}

page_body = load_page(current_url);

fn extract_link_info(line :: String) -> Array {
    close_idx :: Int64 = -1;
    len :: Int64 = line.length();
    through i :: 6..(len - 1) -> loop {
        if (line[i] == "]") { close_idx = i; break; }
    };
    if (close_idx == -1) { return ["", ""]; }
    return [line.substr(6, close_idx - 6), line.substr(close_idx + 1, len - close_idx - 1)];
}

# ====================================================================
# CLI OVERLAY COMMAND ROUTER
# ====================================================================
fn dispatch_cli_command(raw_input :: String) {
    clean_cmd = clean_str(raw_input);
    if (clean_cmd == "") { return null; }

    cli_logs.push("> " + clean_cmd);
    parsed = parse_input(clean_cmd);
    cmd  = parsed[0];
    args = parsed[1];

    if (cmd == "scan") {
        vnet.send_to(client_sock, server_ip, server_port, "SCAN:REQ");
    }
    else if (cmd == "crack") {
        if (args == "") { cli_logs.push("[ERROR]: Usage: crack <val>"); }
        else { vnet.send_to(client_sock, server_ip, server_port, "CRACK:" + args); }
    }
    else if (cmd == "cat") {
        if (args == "") { cli_logs.push("[ERROR]: Usage: cat <vfs_path>"); }
        else { vnet.send_to(client_sock, server_ip, server_port, "CAT:" + args); }
    }
    else if (cmd == "patch") {
        vnet.send_to(client_sock, server_ip, server_port, "PATCH:SEC_OVERRIDE");
    }
    else if (cmd == "netscan") {
        cli_logs.push("[NETSCAN]: Querying peers on " + current_url + "...");
        vnet.send_to(client_sock, server_ip, server_port, "NETSCAN:" + current_url);
    }
    else if (cmd == "help") {
        cli_logs.push("NETWORK COMMANDS:");
        cli_logs.push("  scan             - Query central firewall & salt");
        cli_logs.push("  crack <val>      - Execute hash breach attempt");
        cli_logs.push("  cat <vfs_path>   - Exfiltrate remote system file");
        cli_logs.push("  netscan          - Scan active peers on current URL");
        cli_logs.push("  patch            - [DEFENDER] Relock firewall & rotate salt");
        cli_logs.push("LOCAL SYSTEM COMMANDS:");
        cli_logs.push("  analyze <file>   - Static code inspection");
        cli_logs.push("  sysinfo          - Print engine & architecture stats");
        cli_logs.push("  memory           - Print working set RAM usage");
        cli_logs.push("  theme <name>     - Switch theme (amber, matrix, cyberpunk)");
        cli_logs.push("  clear            - Wipe overlay terminal log buffer");
    }
    else if (cmd == "analyze") {
        lines = cmd_analyze(args);
        through line :: lines -> loop { cli_logs.push(line); };
    }
    else if (cmd == "sysinfo") {
        cli_logs.push("[SYS]: PLATFORM -> " + string(vcore.platform()));
        cli_logs.push("[SYS]: ENGINE   -> " + string(vcore.engine));
        cli_logs.push("[SYS]: PID      -> " + string(vcore.pid));
    }
    else if (cmd == "memory") {
        mem_mb :: Float64 = vcore.memory_usage / (1024.0 * 1024.0);
        cli_logs.push("[MEM]: WORKING SET: " + string(vmath.round(mem_mb * 100.0) / 100.0) + " MB");
    }
    else if (cmd == "theme") {
        if (args == "matrix" || args == "cyberpunk" || args == "amber") {
            active_theme = get_theme(args);
            cli_logs.push("[THEME]: Applied '" + args + "' scheme.");
        } else {
            cli_logs.push("[ERROR]: Unknown theme. Try amber, matrix, cyberpunk");
        }
    }
    else if (cmd == "clear") {
        cli_logs.clear();
        cli_logs = ["[OVERLAY TERMINAL BUFFER CLEARED]"];
    }
    else {
        cli_logs.push("[ERR]: UNKNOWN COMMAND '" + cmd + "'");
    }

    if (cli_logs.length() > 16) {
        cli_scroll_y = float64(cli_logs.length() - 16) * 22.0;
    }

    return null;
}

# ====================================================================
# MAIN ENGINE LOOP
# ====================================================================
while (vglib.running()) {
    run_time     = run_time + 0.016;
    cursor_blink = cursor_blink + 0.016;

    m_pos   = vglib.mouse_pos();
    mx :: Float64 = float64(m_pos[0]);
    my :: Float64 = float64(m_pos[1]);
    
    m_down  :: Int64 = vglib.mouse_down(vglib.MOUSE_LEFT);
    m_click :: Int64 = (m_down == 1 && mouse_was_down == 0) ? 1 : 0;
    mouse_was_down   = m_down;

    # --- TAB KEY: TOGGLE CLI OVERLAY ---
    if (vglib.key_pressed(vglib.TAB)) {
        cli_overlay_open = (cli_overlay_open == 1) ? 0 : 1;
        glitch_trigger = 0.15;
    }

    # Scroll Wheel Handling (Routes to CLI if open, Browser if closed)
    wheel = vglib.mouse_wheel();
    if (wheel != 0.0) {
        if (cli_overlay_open == 1) {
            cli_scroll_y = vmath.clamp(cli_scroll_y - (wheel * 22.0), 0.0, 3000.0);
        } else {
            scroll_y = vmath.clamp(scroll_y - (wheel * 28.0), 0.0, 1500.0);
        }
    }

    # --- KEYBOARD INPUT DISPATCHING ---
    if (cli_overlay_open == 1) {
        ch = vglib.get_char();
        while (ch != "") {
            cli_input_buffer = cli_input_buffer + ch;
            ch = vglib.get_char();
        }

        if (vglib.key_pressed(vglib.BACKSPACE) && cli_input_buffer.length() > 0) {
            cli_input_buffer = cli_input_buffer.substr(0, cli_input_buffer.length() - 1);
        }

        if (vglib.key_pressed(vglib.ENTER) && cli_input_buffer.length() > 0) {
            dispatch_cli_command(cli_input_buffer);
            cli_input_buffer = "";
        }
    } else {
        # Browser URL Input
        if (m_click == 1) {
            url_focused = (mx >= 120.0 && mx <= 920.0 && my >= 12.0 && my <= 48.0) ? 1 : 0;
        }

        if (url_focused == 1) {
            ch = vglib.get_char();
            while (ch != "") {
                input_url = input_url + ch;
                ch = vglib.get_char();
            }

            if (vglib.key_pressed(vglib.BACKSPACE) && input_url.length() > 0) {
                input_url = input_url.substr(0, input_url.length() - 1);
            }

            if (vglib.key_pressed(vglib.ENTER) && input_url.length() > 0) {
                current_url = input_url;
                page_body   = load_page(current_url);
                scroll_y    = 0.0;

                vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
                trace_level = int64(vmath.clamp(float64(trace_level + 4), 0.0, 100.0));
                glitch_trigger = 0.3;
                url_focused = 0;
            }
        }
    }

    # Non-Blocking UDP Packet Ingest (Always Active!)
    packet_in :: Array = vnet.recv_from(client_sock);
    if (packet_in.length() >= 3) {
        net_msg :: String = string(packet_in[0]);
        cli_logs.push("[NET_IN] " + net_msg);
        server_status = net_msg;
        glitch_trigger = 0.2;
    }

    if (glitch_trigger > 0.0) { glitch_trigger = glitch_trigger - 0.016; }

    # ================================================================
    # RENDER ENGINE
    # ================================================================
    vglib.begin();
        vglib.clear(COLOR_BLACK);

        jitter_x :: Float64 = (glitch_trigger > 0.0) ? (vmath.sin(run_time * 50.0) * 4.0) : 0.0;
        jitter_y :: Float64 = (glitch_trigger > 0.0) ? (vmath.cos(run_time * 30.0) * 3.0) : 0.0;

        # Header Bar
        vglib.rect(0 + jitter_x, 0 + jitter_y, 1280, 60, COLOR_PANEL);
        vglib.line(0, 60, 1280, 60, COLOR_BORDER);
        vglib.text_ex(vcr_font, "SHADOWNET", 15 + jitter_x, 22, 14, COLOR_BLOOD);

        # URL Input Box
        vglib.rect(120 + jitter_x, 12 + jitter_y, 800, 36, COLOR_URLBAR);
        vglib.line(120 + jitter_x, 12 + jitter_y, 920 + jitter_x, 12 + jitter_y, url_focused == 1 ? COLOR_BLOOD : COLOR_BORDER);
        display_url_str :: String = "vnet://" + (url_focused == 1 ? input_url : current_url);
        vglib.text_ex(vcr_font, display_url_str, 135 + jitter_x, 23, 12, url_focused == 1 ? COLOR_TOXIC : COLOR_CYAN);

        # URL Bar Measured Cursor
        if (url_focused == 1 && cli_overlay_open == 0 && vmath.fmod(cursor_blink, 0.8) > 0.4) {
            url_size :: Array = vglib.measure_text(vcr_font, display_url_str, 12.0);
            cur_x :: Float64 = 135.0 + float64(url_size[0]) + 2.0;
            vglib.rect(cur_x + jitter_x, 23 + jitter_y, 8, 14, COLOR_TOXIC);
        }

        # VPN Badge
        vglib.rect(940 + jitter_x, 12 + jitter_y, 320, 36, COLOR_PANEL);
        vglib.text_ex(vcr_font, "[VPN: 4 HOPS ACTIVE]", 955 + jitter_x, 23, 11, COLOR_TOXIC);

        # Browser Viewport Frame
        vglib.rect(20 + jitter_x, 80 + jitter_y, 890, 670, COLOR_PANEL);
        vglib.line(20, 80, 910, 80, COLOR_BORDER);
        vglib.line(910, 80, 910, 750, COLOR_BORDER);

        # Right Sidebar (Threat Radar & Network Telemetry)
        vglib.rect(930 + jitter_x, 80 + jitter_y, 330, 670, COLOR_PANEL);
        vglib.line(930, 80, 1260, 80, COLOR_BORDER);
        vglib.text_ex(vcr_font, "SYSTEM THREAT RADAR", 945 + jitter_x, 100, 12, COLOR_AMBER);
        vglib.line(945, 120, 1245, 120, COLOR_BORDER);

        vglib.text_ex(vcr_font, "TRACE LEVEL GAUGE:", 945 + jitter_x, 135, 11, COLOR_CYAN);
        vglib.rect(945 + jitter_x, 155, 300, 16, COLOR_BLACK);
        bar_w :: Float64 = vmath.clamp((float64(trace_level) / 100.0) * 300.0, 4.0, 300.0);
        vglib.rect(945 + jitter_x, 155, bar_w, 16, (trace_level > 70) ? COLOR_BLOOD : COLOR_AMBER);
        vglib.text_ex(vcr_font, string(trace_level) + "% TRACED BY PEERS", 945 + jitter_x, 178, 10, COLOR_AMBER);

        vglib.line(945, 205, 1245, 205, COLOR_BORDER);
        vglib.text_ex(vcr_font, "NETWORK LOG MONITOR:", 945 + jitter_x, 220, 11, COLOR_CYAN);
        vglib.text_ex(vcr_font, "- NODE 8001 BOUND TO UDP", 945 + jitter_x, 245, 10, COLOR_GHOST);
        vglib.text_ex(vcr_font, "- TARGET: " + current_url, 945 + jitter_x, 265, 10, COLOR_TOXIC);

        vglib.line(945, 300, 1245, 300, COLOR_BORDER);
        vglib.text_ex(vcr_font, "SYSTEM OVERLAY:", 945 + jitter_x, 315, 11, COLOR_BLOOD);
        vglib.text_ex(vcr_font, "PRESS [TAB] TO OPEN/CLOSE", 945 + jitter_x, 340, 10, COLOR_AMBER);
        vglib.text_ex(vcr_font, "REAL-TIME HACK TERMINAL", 945 + jitter_x, 360, 10, COLOR_AMBER);

        # --- VIEWPORT PAGE MARKUP RENDERER ---
        line_idx :: Int64 = 0;
        through line_item :: page_body -> loop {
            line_str :: String = string(line_item);
            y_pos :: Float64 = 105.0 + (float64(line_idx) * 28.0) - scroll_y;
            line_idx = line_idx + 1;

            if (y_pos >= 85.0 && y_pos <= 730.0) {
                if (line_str.length() > 7 && line_str.substr(0, 7) == "[TITLE]") {
                    vglib.text_ex(vcr_font, line_str.substr(8, line_str.length() - 8), 40 + jitter_x, y_pos, 15, COLOR_BLOOD);
                }
                else if (line_str.length() > 10 && line_str.substr(0, 10) == "[SUBTITLE]") {
                    vglib.text_ex(vcr_font, line_str.substr(11, line_str.length() - 11), 40 + jitter_x, y_pos, 12, COLOR_AMBER);
                }
                else if (line_str.length() > 6 && line_str.substr(0, 6) == "[WARN]") {
                    vglib.text_ex(vcr_font, line_str.substr(7, line_str.length() - 7), 40 + jitter_x, y_pos, 11, COLOR_BLOOD);
                }
                else if (line_str.length() > 6 && line_str.substr(0, 6) == "[TEXT]") {
                    vglib.text_ex(vcr_font, line_str.substr(7, line_str.length() - 7), 40 + jitter_x, y_pos, 11, COLOR_GHOST);
                }
                else if (line_str.length() > 6 && line_str.substr(0, 6) == "[CODE]") {
                    vglib.text_ex(vcr_font, line_str.substr(7, line_str.length() - 7), 40 + jitter_x, y_pos, 11, COLOR_TOXIC);
                }
                else if (line_str.length() > 5 && line_str.substr(0, 5) == "[BOX]") {
                    vglib.text_ex(vcr_font, line_str.substr(6, line_str.length() - 6), 40 + jitter_x, y_pos, 11, COLOR_CYAN);
                }
                else if (line_str == "[HR]") {
                    vglib.line(40, y_pos + 12.0, 890, y_pos + 12.0, COLOR_BORDER);
                }
                else if (line_str.length() > 6 && line_str.substr(0, 6) == "[LINK:") {
                    link_info = extract_link_info(line_str);
                    target_url :: String = string(link_info[0]);
                    label_txt  :: String = string(link_info[1]);

                    label_size :: Array = vglib.measure_text(vcr_font, label_txt, 12.0);
                    lbl_w :: Float64 = float64(label_size[0]);

                    is_hover :: Int64 = (cli_overlay_open == 0 && mx >= 40.0 && mx <= (40.0 + lbl_w + 20.0) && my >= y_pos && my <= (y_pos + 22.0)) ? 1 : 0;
                    link_col = (is_hover == 1) ? COLOR_TOXIC : COLOR_CYAN;

                    vglib.text_ex(vcr_font, label_txt, 40 + jitter_x, y_pos, 12, link_col);

                    if (is_hover == 1 && m_click == 1) {
                        current_url = target_url;
                        input_url   = target_url;
                        page_body   = load_page(current_url);
                        scroll_y    = 0.0;

                        vnet.send_to(client_sock, server_ip, server_port, "GET:" + current_url);
                        trace_level = int64(vmath.clamp(float64(trace_level + 6), 0.0, 100.0));
                        glitch_trigger = 0.25;
                    }
                }
            }
        };

        # ================================================================
        # REAL-TIME SLIDE-DOWN CLI TERMINAL OVERLAY
        # ================================================================
        if (cli_overlay_open == 1) {
            vglib.rect(20 + jitter_x, 80 + jitter_y, 890, 520, COLOR_CLI_BG);
            vglib.line(20, 80, 910, 80, COLOR_BLOOD);
            vglib.line(910, 80, 910, 600, COLOR_BLOOD);
            vglib.line(910, 600, 20, 600, COLOR_BLOOD);
            vglib.line(20, 600, 20, 80, COLOR_BLOOD);

            vglib.text_ex(vcr_font, "SYSTEM TERMINAL OVERLAY | REAL-TIME CLI", 35 + jitter_x, 95, 12, COLOR_BLOOD);
            vglib.line(35, 115, 895, 115, COLOR_BORDER);

            # Log Feed Rendering
            log_cnt = cli_logs.length();
            cli_start_y :: Float64 = 125.0 - cli_scroll_y;
            through c_idx :: 0..(log_cnt - 1) -> loop {
                line_y :: Float64 = cli_start_y + (c_idx * 22.0);
                if (line_y >= 120.0 && line_y <= 540.0) {
                    txt = string(cli_logs[c_idx]);
                    col = active_theme.text;
                    if (txt.substr(0, 2) == "> ")       { col = COLOR_TOXIC; }
                    if (txt.substr(0, 8) == "[NET_IN]") { col = COLOR_AMBER; }
                    if (txt.substr(0, 7) == "[ERROR]" || txt.substr(0, 5) == "[ERR]") { col = COLOR_BLOOD; }

                    vglib.text_ex(vcr_font, txt, 35 + jitter_x, line_y, 11, col);
                }
            };

            # Input Prompt Line
            vglib.line(20, 560, 910, 560, COLOR_BORDER);
            prompt_str :: String = "CMD> " + cli_input_buffer;
            vglib.text_ex(vcr_font, prompt_str, 35 + jitter_x, 572, 12, COLOR_TOXIC);

            # Measured Blinking Cursor
            if (vmath.fmod(cursor_blink, 0.8) > 0.4) {
                prompt_size :: Array = vglib.measure_text(vcr_font, prompt_str, 12.0);
                cur_x :: Float64 = 35.0 + float64(prompt_size[0]) + 2.0;
                vglib.rect(cur_x + jitter_x, 572 + jitter_y, 8, 14, COLOR_TOXIC);
            }
        }

        # CRT Scanlines
        through sy :: 0..199 -> loop {
            line_y :: Float64 = float64(sy * 4);
            vglib.line(0, line_y, 1280, line_y, COLOR_SCANLINE);
        };

        # Footer Status Strip
        vglib.rect(0, 765, 1280, 35, COLOR_PANEL);
        vglib.line(0, 765, 1280, 765, COLOR_BORDER);
        vglib.text_ex(vcr_font, "TOR ROUTER ACTIVE | NODE PORT: " + string(my_port) + " | [TAB] TOGGLE TERMINAL OVERLAY", 20, 776, 10, COLOR_CYAN);

    vglib.end();
}

vnet.close(client_sock);
vglib.close();