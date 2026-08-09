ruleset { dynamic_casting };
module vglib;
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
    return Theme(
        vglib.rgba(12, 10, 8, 255),
        vglib.rgba(22, 18, 14, 255),
        vglib.rgba(80, 60, 30, 255),
        vglib.rgba(255, 170, 0, 255),
        vglib.rgba(160, 110, 20, 255),
        vglib.rgba(50, 255, 120, 255)
    );
}

active_theme = get_theme("amber");

COLOR_GREEN = vglib.rgba(50, 255, 120, 255);
COLOR_RED   = vglib.rgba(255, 60, 60, 255);
COLOR_CYAN  = vglib.rgba(0, 240, 255, 255);

vglib.init(1280, 800, 60, "VYNE ADVANCED SYSTEM TERMINAL v2.4", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

# --- TERMINAL & 3D VIEWPORT STATE ---
command_input  :: String  = "";
cursor_blink   :: Float64 = 0.0;
scroll_y       :: Float64 = 0.0;
donut_angle_a  :: Float64 = 0.0;
donut_angle_b  :: Float64 = 0.0;
run_time       :: Float64 = 0.0;

# 3D Model Viewport Handles
active_3d_model :: Int64   = 0;
active_model_name::String  = "";
model_rot_y     :: Float64 = 0.0;
camera_3d       :: Int64   = vglib.camera(45.0);

history_logs :: Array = [
    "[SYSTEM]: VYNE OS KERNEL INITIALIZED",
    "[SYSTEM]: MOUNTING VCORE MODULE (PID: " + string(vcore.pid) + ")",
    "[SYSTEM]: GRAPHICS ENGINE READY - VGLIB v0.0.4-alpha",
    "Type 'help' to see available terminal commands.",
    "--------------------------------------------------"
];

# --- STRING SANITIZER ---
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
    interface_count  :: Int64 = 0;
    loop_count       :: Int64 = 0;
    if_count         :: Int64 = 0;
    module_count     :: Int64 = 0;
    decision_points  :: Int64 = 1;

    current_line :: String = "";

    through i :: 0..(source.length() - 1) -> loop {
        ch = source.substr(i, 1);

        if (ch == "\n" || i == (source.length() - 1)) {
            if (ch != "\n" && ch != "\r") { 
                current_line = current_line + ch; 
            }

            line_count++;
            
            trimmed = clean_str(current_line);

            if (trimmed.length() == 0) {
                blank_lines++;
            } else if (trimmed.substr(0, 1) == "#") {
                comment_lines++;
            } else {
                code_lines++;

                if (trimmed.substr(0, 3) == "fn ")         { fn_count++; }
                if (trimmed.substr(0, 10) == "interface ") { interface_count++; }
                if (trimmed.substr(0, 7) == "module ")     { module_count++; }
                if (trimmed.substr(0, 4) == "use ")        { module_count++; }

                if (trimmed.substr(0, 3) == "if ") {
                    if_count++;
                    decision_points++;
                }
                if (trimmed.substr(0, 6) == "while " || trimmed.substr(0, 8) == "through ") {
                    loop_count++;
                    decision_points++;
                }
            }

            current_line = "";
        } else {
            if (ch != "\r") {
                current_line = current_line + ch;
            }
        }
    };

    comment_ratio :: Float64 = 0.0;
    if (line_count > 0) {
        comment_ratio = vmath.round((float64(comment_lines) / float64(line_count)) * 100.0);
    }

    results.push("==================================================");
    results.push(" [ANALYSIS REPORT]: " + target);
    results.push("--------------------------------------------------");
    results.push("  FILE SIZE:        " + string(file_bytes) + " bytes");
    results.push("  TOTAL LINES:      " + string(line_count));
    results.push("  EFFECTIVE LOC:    " + string(code_lines));
    results.push("  COMMENTS (#):     " + string(comment_lines) + " (" + string(comment_ratio) + "%)");
    results.push("  BLANK LINES:      " + string(blank_lines));
    results.push("--------------------------------------------------");
    results.push("  FUNCTIONS (fn):   " + string(fn_count));
    results.push("  INTERFACES:       " + string(interface_count));
    results.push("  IMPORTS/MODULES:  " + string(module_count));
    results.push("  LOOPS (while/for):" + string(loop_count));
    results.push("  BRANCHES (if):    " + string(if_count));
    results.push("  CYCLOMATIC CPLX:  " + string(decision_points));
    results.push("==================================================");
    return results;
}

# ====================================================================
# FAST 3D MESH INSPECTOR & ANALYZER (RENDER3D <FILE.OBJ>)
# ====================================================================
fn cmd_render3d(args :: String) -> Array {
    results :: Array = [];
    target = clean_str(args);

    if (target == "") {
        results.push("[ERROR]: Usage: render3d <model.obj>");
        return results;
    }

    if (!vfs.exists(target)) {
        results.push("[ERROR]: Model file '" + target + "' not found.");
        return results;
    }

    file_bytes :: Int64 = vfs.file_size(target);

    # 1. Load Native Model directly into GPU (Compiled C++ Raylib Engine)
    model_ptr = vglib.load_model(target);
    if (model_ptr == 0) {
        results.push("[ERROR]: Failed to compile 3D model geometry.");
        return results;
    }

    active_3d_model   = model_ptr;
    active_model_name = vfs.filename(target);

    # 2. Fast O(N) Index Windowing Telemetry (No character-by-character string concatenation)
    v_count  :: Int64 = 0;
    vt_count :: Int64 = 0;
    vn_count :: Int64 = 0;
    f_count  :: Int64 = 0;

    obj_data :: String = vfs.read(target);
    data_len :: Int64  = obj_data.length();
    line_start :: Int64 = 0;

    through i :: 0..(data_len - 1) -> loop {
        ch = obj_data.substr(i, 1);

        if (ch == "\n" || i == (data_len - 1)) {
            line_len :: Int64 = (ch == "\n") ? (i - line_start) : (i - line_start + 1);

            if (line_len >= 2) {
                # Slice prefix directly from source string without assembling current_line
                p3 = obj_data.substr(line_start, 3);
                p2 = p3.substr(0, 2);

                if (p2 == "v ")  { v_count++; }
                if (p2 == "f ")  { f_count++; }
                if (p3 == "vt ") { vt_count++; }
                if (p3 == "vn ") { vn_count++; }
            }

            line_start = i + 1;
        }
    };

    results.push("==================================================");
    results.push(" [3D MESH INSPECTOR]: " + active_model_name);
    results.push("--------------------------------------------------");
    results.push("  FILE SIZE:        " + string(file_bytes) + " bytes");
    results.push("  VERTICES (v):     " + string(v_count));
    results.push("  TEXTURE COORDS:   " + string(vt_count));
    results.push("  NORMALS (vn):     " + string(vn_count));
    results.push("  POLYGONS (f):     " + string(f_count));
    results.push("--------------------------------------------------");
    results.push(" [STATUS]: Model loaded to GPU Viewport Panel!");
    results.push(" Use 'unload3d' to close 3D renderer.");
    results.push("==================================================");

    return results;
}

# ====================================================================
# COMMAND DISPATCH ROUTER
# ====================================================================
fn dispatch_command(raw_input :: String) {
    clean_cmd = clean_str(raw_input);
    if (clean_cmd == "") { return null; }

    history_logs.push("> " + clean_cmd);

    parsed = parse_input(clean_cmd);
    cmd  = parsed[0];
    args = parsed[1];

    if (cmd == "help") {
        history_logs.push("AVAILABLE COMMANDS:");
        history_logs.push("  help             - Show command roster");
        history_logs.push("  analyze <file>   - Static analysis on .vy script");
        history_logs.push("  render3d <file>  - Analyze & render 3D .obj mesh in viewport");
        history_logs.push("  unload3d         - Unload 3D mesh and return to ASCII donut");
        history_logs.push("  sysinfo          - Display hardware & architecture specs");
        history_logs.push("  memory           - Print resident physical RAM usage");
        history_logs.push("  uptime           - Show active session duration");
        history_logs.push("  theme <name>     - Switch theme (amber, matrix, cyberpunk)");
        history_logs.push("  echo <text>      - Output text to console");
        history_logs.push("  clear            - Wipe terminal buffer screen");
        history_logs.push("  exit             - Shutdown system");
    } else if (cmd == "sysinfo") {
        history_logs.push("[SYS]: ENGINE   -> " + string(vcore.engine));
        history_logs.push("[SYS]: TARGET   -> " + string(vcore.platform()));
        history_logs.push("[SYS]: BUILD    -> " + string(vcore.build));
        history_logs.push("[SYS]: CORES    -> " + string(vcore.processor_count));
        history_logs.push("[SYS]: PID      -> " + string(vcore.pid));
    } else if (cmd == "memory") {
        mem_mb :: Float64 = vcore.memory_usage / (1024.0 * 1024.0);
        history_logs.push("[MEM]: RESIDENT WORKING SET: " + string(vmath.round(mem_mb * 100.0) / 100.0) + " MB");
    } else if (cmd == "uptime") {
        sec :: Int64 = vmath.round(run_time);
        history_logs.push("[UPTIME]: Active for " + string(sec) + " seconds");
    } else if (cmd == "theme") {
        if (args == "matrix" || args == "cyberpunk" || args == "amber") {
            active_theme = get_theme(args);
            history_logs.push("[THEME]: Applied '" + args + "' color scheme.");
        } else {
            history_logs.push("[ERROR]: Unknown theme. Available: amber, matrix, cyberpunk");
        }
    } else if (cmd == "echo") {
        history_logs.push(args);
    } else if (cmd == "clear") {
        history_logs.clear();
        scroll_y = 0.0;
        history_logs = [
            "[SYSTEM]: VYNE OS KERNEL INITIALIZED",
            "[SYSTEM]: MOUNTING VCORE MODULE (PID: " + string(vcore.pid) + ")",
            "[SYSTEM]: GRAPHICS ENGINE READY - VGLIB v0.0.4-alpha",
            "Type 'help' to see available terminal commands.",
            "--------------------------------------------------"
        ];
    } else if (cmd == "exit") {
        vglib.close();
    } else if (cmd == "analyze") {
        lines = cmd_analyze(args);
        through line :: lines -> loop { history_logs.push(line); };
    } else if (cmd == "render3d") {
        lines = cmd_render3d(args);
        through line :: lines -> loop { history_logs.push(line); };
    } else if (cmd == "unload3d") {
        active_3d_model = 0;
        active_model_name = "";
        history_logs.push("[3D]: Viewport reset to default co-processor.");
    } else {
        history_logs.push("[ERROR]: Unrecognized command '" + cmd + "'");
    }

    if (history_logs.length() > 24) {
        scroll_y = float64(history_logs.length() - 24) * 22.0;
    }

    return null;
}

# ====================================================================
# MAIN ENGINE LOOP
# ====================================================================
while (vglib.running()) {
    run_time = run_time + 0.016;
    cursor_blink = cursor_blink + 0.016;

    donut_angle_a = donut_angle_a + 0.04;
    donut_angle_b = donut_angle_b + 0.02;
    model_rot_y   = model_rot_y + 1.2;

    wheel = vglib.mouse_wheel();
    if (wheel != 0.0) {
        scroll_y = vmath.clamp(scroll_y - (wheel * 20.0), 0.0, 2000.0);
    }

    ch = vglib.get_char();
    while (ch != "") {
        command_input = command_input + ch;
        ch = vglib.get_char();
    }

    if (vglib.key_pressed(vglib.ENTER)) {
        dispatch_command(command_input);
        command_input = "";
    }

    if (vglib.key_pressed(vglib.BACKSPACE)) {
        if (command_input.length() > 0) {
            command_input = command_input.substr(0, command_input.length() - 1);
        }
    }

    vglib.begin();
        vglib.clear(active_theme.bg);

        # Header Status Bar
        vglib.rect(0, 0, 1280, 40, active_theme.panel);
        vglib.line(0, 40, 1280, 40, active_theme.border);
        vglib.text_ex(vcr_font, "VYNE SYSTEM TERMINAL v2.4", 16, 12, 14, active_theme.text);
        vglib.text_ex(vcr_font, "TIME: " + vcore.now(), 450, 12, 12, active_theme.dim);

        # Terminal History Buffer (Left Panel)
        vglib.rect(20, 60, 820, 660, active_theme.panel);
        vglib.line(20, 60, 840, 60, active_theme.border);
        vglib.line(840, 60, 840, 720, active_theme.border);
        vglib.line(840, 720, 20, 720, active_theme.border);
        vglib.line(20, 720, 20, 60, active_theme.border);

        log_count = history_logs.length();
        start_y :: Float64 = 75.0 - scroll_y;

        through i :: 0..(log_count - 1) -> loop {
            line_y :: Float64 = start_y + (i * 22.0);
            if (line_y >= 70.0 && line_y <= 660.0) {
                txt = history_logs[i];
                col = active_theme.text;
                
                if (txt.substr(0, 8) == "[SYSTEM]") { col = COLOR_CYAN; }
                if (txt.substr(0, 7) == "[ERROR]")  { col = COLOR_RED; }
                if (txt.substr(0, 2) == "> ")       { col = COLOR_GREEN; }

                vglib.text_ex(vcr_font, txt, 35, line_y, 11, col);
            }
        };

        # Input Prompt Line
        vglib.line(20, 675, 840, 675, active_theme.border);
        vglib.text_ex(vcr_font, "vyne@root:~$ " + command_input, 35, 690, 12, active_theme.text);

        if (vmath.fmod(cursor_blink, 0.8) > 0.4) {
            cursor_x :: Float64 = 165.0 + (command_input.length() * 9.5);
            vglib.rect(cursor_x, 690, 8, 14, active_theme.text);
        }

        # Telemetry & 3D Viewport Panel (Right Panel)
        vglib.rect(870, 60, 390, 660, active_theme.panel);
        vglib.line(870, 60, 1260, 60, active_theme.border);
        vglib.line(1260, 60, 1260, 720, active_theme.border);
        vglib.line(1260, 720, 870, 720, active_theme.border);
        vglib.line(870, 720, 870, 60, active_theme.border);

        vglib.text_ex(vcr_font, "SYSTEM TELEMETRY", 890, 80, 12, active_theme.text);
        vglib.line(890, 100, 1240, 100, active_theme.border);

        vglib.text_ex(vcr_font, "HOST OS:  " + string(vcore.platform()), 890, 115, 10, active_theme.dim);
        vglib.text_ex(vcr_font, "PID:      " + string(vcore.pid), 890, 135, 10, active_theme.dim);
        vglib.text_ex(vcr_font, "CORES:    " + string(vcore.processor_count), 890, 155, 10, active_theme.dim);

        mem_mb :: Float64 = vcore.memory_usage / (1024.0 * 1024.0);
        vglib.text_ex(vcr_font, "RAM: " + string(vmath.round(mem_mb)) + " MB", 890, 185, 10, active_theme.text);
        vglib.rect(890, 205, 350, 12, vglib.rgba(35, 28, 20, 255));
        
        bar_fill :: Float64 = vmath.clamp((mem_mb / 512.0) * 350.0, 10.0, 350.0);
        vglib.rect(890, 205, bar_fill, 12, active_theme.text);

        vglib.line(890, 240, 1240, 240, active_theme.border);

        # DYNAMIC VIEWPORT MODE SWITCHER (3D Mesh vs ASCII Donut)
        if (active_3d_model != 0) {
            vglib.text_ex(vcr_font, "3D GPU VIEWPORT [" + active_model_name + "]", 890, 255, 11, COLOR_GREEN);
            
            # Position 3D Camera for Panel Orbiting
            cam_x = vmath.sin(model_rot_y * 0.02) * 6.0;
            cam_z = vmath.cos(model_rot_y * 0.02) * 6.0;
            vglib.set_pos(camera_3d, cam_x, 3.5, cam_z);

            # Render 3D Scene
            vglib.begin3d(camera_3d);
                vglib.grid(10, 1.0);
                vglib.draw_model(active_3d_model, 0.0, 0.0, 0.0, 1.0, COLOR_CYAN);
            vglib.end3d();
        } else {
            vglib.text_ex(vcr_font, "MATH CO-PROCESSOR [DONUT.C]", 890, 255, 11, COLOR_CYAN);
            vglib.donut(donut_angle_a, donut_angle_b);
        }

        # Footer Status Strip
        vglib.rect(0, 740, 1280, 60, vglib.rgba(16, 12, 10, 255));
        vglib.line(0, 740, 1280, 740, active_theme.border);
        vglib.text_ex(vcr_font, "STATUS: ONLINE | INPUT: UTF8-ACTIVE | SCROLL: " + string(vmath.round(scroll_y)), 20, 762, 10, active_theme.dim);

    vglib.end();
}

vglib.close();