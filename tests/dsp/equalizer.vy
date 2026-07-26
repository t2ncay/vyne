ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1000, 600, 60, "Vyne Pro-Q Equalizer", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");
is_ready = vaudio.init_audio();
vaudio.volume(1.0);

track = vaudio.load_sound("tests/assets/fucking hardshit.wav");
vaudio.play_sound(track);

# ATTACH C++ DSP PROCESSOR
vaudio.attach_eq(track);

# 4 EQ Bands: [Freq (Hz), Gain (dB), Q]
b1_f :: Float64 = 100.0;   b1_g :: Float64 = 3.0;   b1_q :: Float64 = 1.0;
b2_f :: Float64 = 1000.0;  b2_g :: Float64 = -4.0;  b2_q :: Float64 = 1.4;
b3_f :: Float64 = 3500.0;  b3_g :: Float64 = 5.0;   b3_q :: Float64 = 1.2;
b4_f :: Float64 = 10000.0; b4_g :: Float64 = 2.0;   b4_q :: Float64 = 0.8;

enabled :: Int64 = 1;
active_node = 0;

# --- LOGARITHMIC FREQUENCY MAPPING (20Hz - 20000Hz -> 60px to 900px) ---
fn freq_to_x(f) {
    min_f = 20.0;
    max_f = 20000.0;
    
    log_min = 1.30103;  # log10(20)
    log_max = 4.30103;  # log10(20000)
    
    log_val = vmath.log(vmath.clamp(f, min_f, max_f)) / 2.302585; # natural log to log10
    norm = (log_val - log_min) / (log_max - log_min);
    return 60.0 + (norm * 840.0);
}

fn x_to_freq(x) {
    log_min = 1.30103;
    log_max = 4.30103;
    
    norm = vmath.clamp((x - 60.0) / 840.0, 0.0, 1.0);
    log_val = log_min + (norm * (log_max - log_min));
    return vmath.pow(10.0, log_val);
}

fn gain_to_y(g) {
    norm = vmath.clamp(g / 18.0, -1.0, 1.0);
    return 260.0 - (norm * 140.0);
}

fn y_to_gain(y) {
    norm = vmath.clamp((260.0 - y) / 140.0, -1.0, 1.0);
    return norm * 18.0;
}

fn draw_eq_node(id_str, x, y, color, is_selected) {
    radius = 12.0;
    if (is_selected) { radius = 16.0; }

    # Outer Halo & Core Node
    vglib.circle(x, y, radius + 3.0, vglib.BLACK);
    vglib.circle(x, y, radius, color);
    vglib.circle(x, y, radius - 4.0, vglib.rgba(18, 20, 26, 255));
    vglib.text_ex(vcr_font, id_str, x - 4.0, y - 5.0, 12, color);
}

fn draw_bypass_button(x, y, is_active) {
    bg_color = vglib.rgba(30, 34, 44, 255);
    btn_color = vglib.rgba(220, 50, 50, 255); # BYPASS (Red)
    label = "BYPASS";

    if (is_active == 1) {
        btn_color = vglib.rgba(0, 220, 255, 255); # ACTIVE (FabFilter Cyan Glow)
        label = "ACTIVE";
    }

    vglib.rect(x, y, 100, 32, bg_color);
    vglib.rect(x + 2, y + 2, 96, 28, btn_color);
    vglib.text_ex(vcr_font, label, x + 16, y + 8, 12, vglib.BLACK);
}

while (vglib.running()) {
    if (vaudio.is_playing(track) == 0) { vaudio.play_sound(track); }

    m = vglib.mouse_pos();
    md = vglib.mouse_delta();

    # --- NODE DRAGGING INTERACTION ---
    if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
        if (active_node == 0) {
            if (vmath.hypot(m[0] - freq_to_x(b1_f), m[1] - gain_to_y(b1_g)) < 22) { active_node = 1; }
            if (vmath.hypot(m[0] - freq_to_x(b2_f), m[1] - gain_to_y(b2_g)) < 22) { active_node = 2; }
            if (vmath.hypot(m[0] - freq_to_x(b3_f), m[1] - gain_to_y(b3_g)) < 22) { active_node = 3; }
            if (vmath.hypot(m[0] - freq_to_x(b4_f), m[1] - gain_to_y(b4_g)) < 22) { active_node = 4; }
        }

        if (active_node == 1) { b1_f = x_to_freq(m[0]); b1_g = y_to_gain(m[1]); }
        if (active_node == 2) { b2_f = x_to_freq(m[0]); b2_g = y_to_gain(m[1]); }
        if (active_node == 3) { b3_f = x_to_freq(m[0]); b3_g = y_to_gain(m[1]); }
        if (active_node == 4) { b4_f = x_to_freq(m[0]); b4_g = y_to_gain(m[1]); }
    } else {
        active_node = 0;
    }

    # Spacebar Bypass
    if (vglib.key_pressed(vglib.SPACE)) {
        if (enabled == 1) { enabled = 0; } else { enabled = 1; }
    }

    # Mouse Click Bypass
    if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
        if (m[0] >= 50 && m[0] <= 150 && m[1] >= 540 && m[1] <= 572) {
            if (enabled == 1) { enabled = 0; } else { enabled = 1; }
        }
    }

    # --- UPDATE C++ AUDIO DSP PARAMETERS ---
    vaudio.enable_eq(enabled);
    vaudio.set_eq(0, b1_f, b1_g, b1_q);
    vaudio.set_eq(1, b2_f, b2_g, b2_q);
    vaudio.set_eq(2, b3_f, b3_g, b3_q);
    vaudio.set_eq(3, b4_f, b4_g, b4_q);

    rms_val = vaudio.get_rms();

    vglib.begin();
        vglib.clear(vglib.rgba(12, 14, 18, 255)); # FabFilter Midnight Chassis

        # --- GRAPH FRAME & GRID ---
        vglib.rect(60, 80, 840, 360, vglib.rgba(18, 22, 30, 255));
        
        # Outer Frame Borders
        vglib.line(60, 80, 900, 80, vglib.rgba(40, 48, 62, 255));
        vglib.line(900, 80, 900, 440, vglib.rgba(40, 48, 62, 255));
        vglib.line(900, 440, 60, 440, vglib.rgba(40, 48, 62, 255));
        vglib.line(60, 440, 60, 80, vglib.rgba(40, 48, 62, 255));

        # Center 0dB Reference Line
        vglib.line(60, 260, 900, 260, vglib.rgba(70, 82, 100, 255));
        vglib.text_ex(vcr_font, "0 dB", 20, 254, 10, vglib.rgba(120, 130, 150, 255));
        vglib.text_ex(vcr_font, "+18dB", 15, 80, 10, vglib.rgba(120, 130, 150, 255));
        vglib.text_ex(vcr_font, "-18dB", 15, 430, 10, vglib.rgba(120, 130, 150, 255));

        # Logarithmic Frequency Grid Lines (100Hz, 1kHz, 10kHz)
        vglib.line(freq_to_x(100.0), 80, freq_to_x(100.0), 440, vglib.rgba(32, 38, 50, 255));
        vglib.text_ex(vcr_font, "100Hz", freq_to_x(100.0) - 15, 448, 10, vglib.rgba(100, 110, 130, 255));

        vglib.line(freq_to_x(1000.0), 80, freq_to_x(1000.0), 440, vglib.rgba(32, 38, 50, 255));
        vglib.text_ex(vcr_font, "1kHz", freq_to_x(1000.0) - 12, 448, 10, vglib.rgba(100, 110, 130, 255));

        vglib.line(freq_to_x(10000.0), 80, freq_to_x(10000.0), 440, vglib.rgba(32, 38, 50, 255));
        vglib.text_ex(vcr_font, "10kHz", freq_to_x(10000.0) - 15, 448, 10, vglib.rgba(100, 110, 130, 255));

        # --- 3. DRAW LIVE EQ FREQUENCY RESPONSE CURVE ---
        prev_px = 60.0;
        prev_py = 260.0;

        step = 8.0;
        curr_x = 60.0;

        while (curr_x <= 900.0) {
            eval_f = x_to_freq(curr_x);

            # Log-distance influence formula for natural peaking curve
            dist1 = vmath.log(eval_f / b1_f);
            g1_contrib = b1_g * vmath.exp(-3.0 * dist1 * dist1 * b1_q);

            dist2 = vmath.log(eval_f / b2_f);
            g2_contrib = b2_g * vmath.exp(-3.0 * dist2 * dist2 * b2_q);

            dist3 = vmath.log(eval_f / b3_f);
            g3_contrib = b3_g * vmath.exp(-3.0 * dist3 * dist3 * b3_q);

            dist4 = vmath.log(eval_f / b4_f);
            g4_contrib = b4_g * vmath.exp(-3.0 * dist4 * dist4 * b4_q);

            tot_db = g1_contrib + g2_contrib + g3_contrib + g4_contrib;
            if (enabled == 0) { tot_db = 0.0; }

            curr_y = gain_to_y(tot_db);

            curve_color = vglib.rgba(0, 230, 255, 255); # FabFilter Cyan
            if (enabled == 0) { curve_color = vglib.rgba(80, 90, 105, 255); }

            if (curr_x > 60.0) {
                vglib.line(prev_px, prev_py, curr_x, curr_y, curve_color);
            }

            prev_px = curr_x;
            prev_py = curr_y;
            curr_x = curr_x + step;
        }

        # --- INTERACTIVE NODES ---
        c1 = vglib.rgba(255, 80, 80, 255);   # Low Red
        c2 = vglib.rgba(255, 200, 50, 255);  # Mid Yellow
        c3 = vglib.rgba(50, 220, 120, 255);  # High-Mid Green
        c4 = vglib.rgba(180, 90, 255, 255);  # Air Purple

        draw_eq_node("1", freq_to_x(b1_f), gain_to_y(b1_g), c1, active_node == 1);
        draw_eq_node("2", freq_to_x(b2_f), gain_to_y(b2_g), c2, active_node == 2);
        draw_eq_node("3", freq_to_x(b3_f), gain_to_y(b3_g), c3, active_node == 3);
        draw_eq_node("4", freq_to_x(b4_f), gain_to_y(b4_g), c4, active_node == 4);

        # --- BOTTOM PARAMETER CARDS ---
        vglib.rect(50, 475, 190, 55, vglib.rgba(20, 24, 32, 255));
        vglib.text_ex(vcr_font, "BAND 1 (LOW)", 60, 482, 11, c1);
        vglib.text_ex(vcr_font, "F: " + string(vmath.round(b1_f)) + "Hz", 60, 498, 10, vglib.WHITE);
        vglib.text_ex(vcr_font, "G: " + string(vmath.round(b1_g)) + "dB", 60, 512, 10, vglib.WHITE);

        vglib.rect(260, 475, 190, 55, vglib.rgba(20, 24, 32, 255));
        vglib.text_ex(vcr_font, "BAND 2 (MID)", 270, 482, 11, c2);
        vglib.text_ex(vcr_font, "F: " + string(vmath.round(b2_f)) + "Hz", 270, 498, 10, vglib.WHITE);
        vglib.text_ex(vcr_font, "G: " + string(vmath.round(b2_g)) + "dB", 270, 512, 10, vglib.WHITE);

        vglib.rect(470, 475, 190, 55, vglib.rgba(20, 24, 32, 255));
        vglib.text_ex(vcr_font, "BAND 3 (HI-MID)", 480, 482, 11, c3);
        vglib.text_ex(vcr_font, "F: " + string(vmath.round(b3_f)) + "Hz", 480, 498, 10, vglib.WHITE);
        vglib.text_ex(vcr_font, "G: " + string(vmath.round(b3_g)) + "dB", 480, 512, 10, vglib.WHITE);

        vglib.rect(680, 475, 190, 55, vglib.rgba(20, 24, 32, 255));
        vglib.text_ex(vcr_font, "BAND 4 (AIR)", 690, 482, 11, c4);
        vglib.text_ex(vcr_font, "F: " + string(vmath.round(b4_f)) + "Hz", 690, 498, 10, vglib.WHITE);
        vglib.text_ex(vcr_font, "G: " + string(vmath.round(b4_g)) + "dB", 690, 512, 10, vglib.WHITE);

        # RMS Output Level Meter
        vglib.rect(925, 80, 18, 360, vglib.rgba(20, 24, 32, 255));
        rms_h = vmath.clamp(rms_val * 360.0, 0.0, 360.0);
        if (rms_h > 1.0) {
            vglib.rect(925, 440 - rms_h, 18, rms_h, vglib.rgba(0, 230, 255, 255));
        }
        vglib.text_ex(vcr_font, "OUT", 920, 448, 10, vglib.rgba(0, 230, 255, 255));

        # --- BYPASS BUTTON & HEADERS ---
        draw_bypass_button(50, 540, enabled);

        vglib.rect(50, 50, 890, 2, vglib.rgba(0, 230, 255, 180));
        vglib.text_ex(vcr_font, "VYNE PRO-Q VISUAL EQUALIZER", 270, 20, 20, vglib.WHITE);
        vglib.text_ex(vcr_font, "Vyne Studio Suite v0.0.1", 720, 550, 11, vglib.rgba(0, 230, 255, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();