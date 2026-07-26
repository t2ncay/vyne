ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

# Window Setup (1200x900 for full Master Rack resolution)
vglib.init(1200, 900, 60, "VYNE MASTER STUDIO RACK v1.0", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

is_ready = vaudio.init_audio();
vaudio.volume(1.0);

# Load Main Audio Track
track = vaudio.load_sound("tests/assets/fucking hardshit.wav");
vaudio.play_sound(track);

# --- ATTACH FULL MASTER DSP CHAIN ---
vaudio.attach_eq(track);
vaudio.attach_compressor(track);
vaudio.attach_saturator(track);
vaudio.attach_reverb(track);

run_time = 0.0;
active_tab :: Int64 = 0; # 0 = EQ, 1 = COMPRESSOR, 2 = SATURATOR, 3 = REVERB

# --- 1. EQUALIZER STATE ---
b1_f :: Float64 = 100.0;   b1_g :: Float64 = 3.0;   b1_q :: Float64 = 1.0;
b2_f :: Float64 = 1000.0;  b2_g :: Float64 = -4.0;  b2_q :: Float64 = 1.4;
b3_f :: Float64 = 3500.0;  b3_g :: Float64 = 5.0;   b3_q :: Float64 = 1.2;
b4_f :: Float64 = 10000.0; b4_g :: Float64 = 2.0;   b4_q :: Float64 = 0.8;
eq_on :: Int64 = 1;
active_eq_node = 0;

# --- 2. COMPRESSOR STATE ---
thresh  :: Float64 = -18.0;
ratio   :: Float64 = 4.0;
attack  :: Float64 = 15.0;
release :: Float64 = 120.0;
makeup  :: Float64 = 3.0;
comp_on :: Int64   = 1;
active_comp_knob = 0;

# --- 3. SATURATOR STATE ---
drive   :: Float64 = 0.45;
sat_mode:: Int64   = 0;
sat_on  :: Int64   = 1;
active_sat_knob = 0;

# --- 4. REVERB STATE ---
decay    :: Float64 = 0.85;
mix      :: Float64 = 0.35;
predelay :: Float64 = 20.0;
damping  :: Float64 = 0.30;
rev_on   :: Int64   = 1;
active_rev_knob = 0;

# --- UI HELPER FUNCTIONS ---
fn draw_knob(name, x, y, val_norm, display_val, color) {
    vglib.circle(x, y, 42.0, vglib.BLACK);
    vglib.circle(x, y, 39.0, vglib.rgba(45, 48, 58, 255));
    vglib.circle(x, y, 35.0, vglib.rgba(22, 24, 30, 255));
    vglib.circle(x, y, 22.0, vglib.rgba(32, 35, 44, 255));
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x :: Float64 = x + vmath.sin(rad) * 31.0;
    line_y :: Float64 = y - vmath.cos(rad) * 31.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 3.5, color);
    
    vglib.text_ex(vcr_font, name, x - 26, y + 48, 13, vglib.WHITE);
    vglib.text_ex(vcr_font, display_val, x - 20, y - 4, 11, color);
}

fn draw_bypass_button(x, y, is_active, active_color) {
    bg_color = vglib.rgba(35, 38, 48, 255);
    btn_color = vglib.rgba(220, 50, 50, 255);
    label = "BYPASS";

    if (is_active == 1) {
        btn_color = active_color;
        label = "ACTIVE";
    }

    vglib.rect(x, y, 110, 34, bg_color);
    vglib.rect(x + 2, y + 2, 106, 30, btn_color);
    vglib.text_ex(vcr_font, label, x + 18, y + 9, 13, vglib.BLACK);
}

fn freq_to_x(f) {
    log_min = 1.30103; log_max = 4.30103;
    log_val = vmath.log(vmath.clamp(f, 20.0, 20000.0)) / 2.302585;
    norm = (log_val - log_min) / (log_max - log_min);
    return 80.0 + (norm * 1040.0);
}

fn x_to_freq(x) {
    log_min = 1.30103; log_max = 4.30103;
    norm = vmath.clamp((x - 80.0) / 1040.0, 0.0, 1.0);
    log_val = log_min + (norm * (log_max - log_min));
    return vmath.pow(10.0, log_val);
}

fn gain_to_y(g) {
    norm = vmath.clamp(g / 18.0, -1.0, 1.0);
    return 420.0 - (norm * 200.0);
}

fn y_to_gain(y) {
    norm = vmath.clamp((420.0 - y) / 200.0, -1.0, 1.0);
    return norm * 18.0;
}

# --- MAIN RACK LOOP ---
while (vglib.running()) {
    run_time = run_time + 0.016;

    if (vaudio.is_playing(track) == 0) {
        vaudio.play_sound(track);
    }

    m = vglib.mouse_pos();
    md = vglib.mouse_delta();
    mouse_click = vglib.mouse_down(vglib.MOUSE_LEFT);

    # --- 1. RACK SWITCHING TAB INTERACTION ---
    if (mouse_click && prev_mouse_state == 0) {
        if (m[1] >= 20 && m[1] <= 60) {
            if (m[0] >= 50 && m[0] <= 200) { active_tab = 0; }  # EQ
            if (m[0] >= 210 && m[0] <= 360) { active_tab = 1; } # COMPRESSOR
            if (m[0] >= 370 && m[0] <= 520) { active_tab = 2; } # SATURATOR
            if (m[0] >= 530 && m[0] <= 680) { active_tab = 3; } # REVERB
        }
    }
    prev_mouse_state = mouse_click ? 1 : 0;

    # --- 2. UPDATE ALL DSP PROCESSORS IN REAL-TIME ---
    vaudio.enable_eq(eq_on);
    vaudio.set_eq(0, b1_f, b1_g, b1_q);
    vaudio.set_eq(1, b2_f, b2_g, b2_q);
    vaudio.set_eq(2, b3_f, b3_g, b3_q);
    vaudio.set_eq(3, b4_f, b4_g, b4_q);

    vaudio.set_compressor(thresh, ratio, attack, release, makeup, comp_on);
    vaudio.set_dsp(drive, sat_mode);
    vaudio.set_reverb(decay, mix, rev_on);

    gr_db   = vaudio.get_gr();
    rms_val = vaudio.get_rms();

    vglib.begin();
        vglib.clear(vglib.rgba(12, 14, 18, 255)); # Studio Chassis Dark

        # --- TOP MASTER RACK TAB BAR ---
        c_eq   = (active_tab == 0) ? vglib.rgba(0, 220, 255, 255)   : vglib.rgba(30, 35, 45, 255);
        c_comp = (active_tab == 1) ? vglib.rgba(255, 120, 40, 255)  : vglib.rgba(30, 35, 45, 255);
        c_sat  = (active_tab == 2) ? vglib.rgba(255, 90, 90, 255)   : vglib.rgba(30, 35, 45, 255);
        c_rev  = (active_tab == 3) ? vglib.rgba(160, 90, 255, 255)  : vglib.rgba(30, 35, 45, 255);

        vglib.rect(50, 20, 150, 40, c_eq);
        vglib.text_ex(vcr_font, "1. PRO-Q EQ", 70, 32, 13, (active_tab == 0) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(210, 20, 150, 40, c_comp);
        vglib.text_ex(vcr_font, "2. COMPRESSOR", 220, 32, 13, (active_tab == 1) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(370, 20, 150, 40, c_sat);
        vglib.text_ex(vcr_font, "3. SATURATOR", 382, 32, 13, (active_tab == 2) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(530, 20, 150, 40, c_rev);
        vglib.text_ex(vcr_font, "4. REVERB", 552, 32, 13, (active_tab == 3) ? vglib.BLACK : vglib.WHITE);

        # Header Title
        vglib.text_ex(vcr_font, "VYNE MASTER STUDIO BUS", 780, 28, 20, vglib.WHITE);
        vglib.line(50, 65, 1150, 65, vglib.rgba(60, 70, 85, 255));

        # ====================================================================
        # RACK 1: PRO-Q EQUALIZER DISPLAY
        # ====================================================================
        if (active_tab == 0) {
            # EQ Node Dragging
            if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
                if (active_eq_node == 0) {
                    if (vmath.hypot(m[0] - freq_to_x(b1_f), m[1] - gain_to_y(b1_g)) < 22) { active_eq_node = 1; }
                    if (vmath.hypot(m[0] - freq_to_x(b2_f), m[1] - gain_to_y(b2_g)) < 22) { active_eq_node = 2; }
                    if (vmath.hypot(m[0] - freq_to_x(b3_f), m[1] - gain_to_y(b3_g)) < 22) { active_eq_node = 3; }
                    if (vmath.hypot(m[0] - freq_to_x(b4_f), m[1] - gain_to_y(b4_g)) < 22) { active_eq_node = 4; }
                }

                if (active_eq_node == 1) { b1_f = x_to_freq(m[0]); b1_g = y_to_gain(m[1]); }
                if (active_eq_node == 2) { b2_f = x_to_freq(m[0]); b2_g = y_to_gain(m[1]); }
                if (active_eq_node == 3) { b3_f = x_to_freq(m[0]); b3_g = y_to_gain(m[1]); }
                if (active_eq_node == 4) { b4_f = x_to_freq(m[0]); b4_g = y_to_gain(m[1]); }
            } else { active_eq_node = 0; }

            if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
                if (m[0] >= 50 && m[0] <= 160 && m[1] >= 810 && m[1] <= 844) {
                    if (eq_on == 1) { eq_on = 0; } else { eq_on = 1; }
                }
            }

            vglib.rect(80, 120, 1040, 500, vglib.rgba(18, 22, 30, 255));
            vglib.line(80, 370, 1120, 370, vglib.rgba(70, 82, 100, 255));

            # Curve
            prev_px :: Float64 = 80.0; prev_py :: Float64 = 370.0;
            curr_x :: Float64 = 80.0;
            while (curr_x <= 1120.0) {
                eval_f = x_to_freq(curr_x);
                d1 = vmath.log(eval_f / b1_f); g1 = b1_g * vmath.exp(-3.0 * d1 * d1 * b1_q);
                d2 = vmath.log(eval_f / b2_f); g2 = b2_g * vmath.exp(-3.0 * d2 * d2 * b2_q);
                d3 = vmath.log(eval_f / b3_f); g3 = b3_g * vmath.exp(-3.0 * d3 * d3 * b3_q);
                d4 = vmath.log(eval_f / b4_f); g4 = b4_g * vmath.exp(-3.0 * d4 * d4 * b4_q);
                tot = (eq_on == 1) ? (g1 + g2 + g3 + g4) : 0.0;
                curr_y = gain_to_y(tot);

                if (curr_x > 80.0) { vglib.line(prev_px, prev_py, curr_x, curr_y, vglib.rgba(0, 230, 255, 255)); }
                prev_px = curr_x; prev_py = curr_y; curr_x = curr_x + 10.0;
            }

            # Draw Nodes
            vglib.circle(freq_to_x(b1_f), gain_to_y(b1_g), 12.0, vglib.rgba(255, 80, 80, 255));
            vglib.circle(freq_to_x(b2_f), gain_to_y(b2_g), 12.0, vglib.rgba(255, 200, 50, 255));
            vglib.circle(freq_to_x(b3_f), gain_to_y(b3_g), 12.0, vglib.rgba(50, 220, 120, 255));
            vglib.circle(freq_to_x(b4_f), gain_to_y(b4_g), 12.0, vglib.rgba(180, 90, 255, 255));

            draw_bypass_button(50, 810, eq_on, vglib.rgba(0, 220, 255, 255));
        }

        # ====================================================================
        # RACK 2: OPTO COMPRESSOR DISPLAY
        # ====================================================================
        if (active_tab == 1) {
            if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
                if (active_comp_knob == 0) {
                    if (vmath.hypot(m[0] - 150, m[1] - 650) < 45) { active_comp_knob = 1; }
                    if (vmath.hypot(m[0] - 350, m[1] - 650) < 45) { active_comp_knob = 2; }
                    if (vmath.hypot(m[0] - 550, m[1] - 650) < 45) { active_comp_knob = 3; }
                    if (vmath.hypot(m[0] - 750, m[1] - 650) < 45) { active_comp_knob = 4; }
                    if (vmath.hypot(m[0] - 950, m[1] - 650) < 45) { active_comp_knob = 5; }
                }

                delta = md[1] * 0.3;
                if (active_comp_knob == 1) { thresh = vmath.clamp(thresh - delta, -40.0, 0.0); }
                if (active_comp_knob == 2) { ratio  = vmath.clamp(ratio - (delta * 0.1), 1.0, 16.0); }
                if (active_comp_knob == 3) { attack = vmath.clamp(attack - delta, 1.0, 100.0); }
                if (active_comp_knob == 4) { release = vmath.clamp(release - (delta * 2.0), 10.0, 500.0); }
                if (active_comp_knob == 5) { makeup = vmath.clamp(makeup - (delta * 0.1), 0.0, 12.0); }
            } else { active_comp_knob = 0; }

            if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
                if (m[0] >= 50 && m[0] <= 160 && m[1] >= 810 && m[1] <= 844) {
                    if (comp_on == 1) { comp_on = 0; } else { comp_on = 1; }
                }
            }

            draw_knob("THRESH", 150, 650, (thresh + 40.0) / 40.0, string(vmath.round(thresh)) + "dB", vglib.rgba(255, 120, 40, 255));
            draw_knob("RATIO", 350, 650, (ratio - 1.0) / 15.0, string(vmath.round(ratio)) + ":1", vglib.rgba(50, 200, 255, 255));
            draw_knob("ATTACK", 550, 650, (attack - 1.0) / 99.0, string(vmath.round(attack)) + "ms", vglib.rgba(255, 220, 50, 255));
            draw_knob("RELEASE", 750, 650, (release - 10.0) / 490.0, string(vmath.round(release)) + "ms", vglib.rgba(180, 100, 255, 255));
            draw_knob("MAKEUP", 950, 650, makeup / 12.0, "+" + string(vmath.round(makeup)) + "dB", vglib.rgba(50, 255, 120, 255));

            draw_bypass_button(50, 810, comp_on, vglib.rgba(255, 120, 40, 255));
        }

        # ====================================================================
        # RACK 3: SATURATOR DISPLAY
        # ====================================================================
        if (active_tab == 2) {
            if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
                if (vmath.hypot(m[0] - 300, m[1] - 400) < 55) {
                    drive = vmath.clamp(drive - (md[1] * 0.003), 0.0, 1.0);
                }
            }

            if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
                if (m[0] >= 550 && m[0] <= 750 && m[1] >= 300 && m[1] <= 340) { sat_mode = 0; }
                if (m[0] >= 550 && m[0] <= 750 && m[1] >= 360 && m[1] <= 400) { sat_mode = 1; }
                if (m[0] >= 550 && m[0] <= 750 && m[1] >= 420 && m[1] <= 460) { sat_mode = 2; }

                if (m[0] >= 50 && m[0] <= 160 && m[1] >= 810 && m[1] <= 844) {
                    if (sat_on == 1) { sat_on = 0; } else { sat_on = 1; }
                }
            }

            draw_knob("DRIVE", 300, 400, drive, string(vmath.round(drive * 100.0)) + "%", vglib.rgba(255, 90, 90, 255));

            vglib.rect(550, 300, 200, 40, (sat_mode == 0) ? vglib.rgba(255, 90, 90, 255) : vglib.rgba(40, 45, 58, 255));
            vglib.text_ex(vcr_font, "SOFT TUBE", 590, 312, 13, (sat_mode == 0) ? vglib.BLACK : vglib.WHITE);

            vglib.rect(550, 360, 200, 40, (sat_mode == 1) ? vglib.rgba(255, 90, 90, 255) : vglib.rgba(40, 45, 58, 255));
            vglib.text_ex(vcr_font, "HARD CLIP", 590, 372, 13, (sat_mode == 1) ? vglib.BLACK : vglib.WHITE);

            vglib.rect(550, 420, 200, 40, (sat_mode == 2) ? vglib.rgba(255, 90, 90, 255) : vglib.rgba(40, 45, 58, 255));
            vglib.text_ex(vcr_font, "ASYMMETRIC", 580, 432, 13, (sat_mode == 2) ? vglib.BLACK : vglib.WHITE);

            draw_bypass_button(50, 810, sat_on, vglib.rgba(255, 90, 90, 255));
        }

        # ====================================================================
        # RACK 4: SPATIAL REVERB DISPLAY
        # ====================================================================
        if (active_tab == 3) {
            if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
                if (active_rev_knob == 0) {
                    if (vmath.hypot(m[0] - 250, m[1] - 400) < 45) { active_rev_knob = 1; }
                    if (vmath.hypot(m[0] - 450, m[1] - 400) < 45) { active_rev_knob = 2; }
                    if (vmath.hypot(m[0] - 650, m[1] - 400) < 45) { active_rev_knob = 3; }
                    if (vmath.hypot(m[0] - 850, m[1] - 400) < 45) { active_rev_knob = 4; }
                }

                delta = md[1] * 0.3;
                if (active_rev_knob == 1) { decay    = vmath.clamp(decay - (delta * 0.01), 0.0, 0.95); }
                if (active_rev_knob == 2) { mix      = vmath.clamp(mix - (delta * 0.01), 0.0, 1.0); }
                if (active_rev_knob == 3) { predelay = vmath.clamp(predelay - delta, 0.0, 100.0); }
                if (active_rev_knob == 4) { damping  = vmath.clamp(damping - (delta * 0.01), 0.0, 1.0); }
            } else { active_rev_knob = 0; }

            if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
                if (m[0] >= 50 && m[0] <= 160 && m[1] >= 810 && m[1] <= 844) {
                    if (rev_on == 1) { rev_on = 0; } else { rev_on = 1; }
                }
            }

            draw_knob("DECAY", 250, 400, decay / 0.95, string(vmath.round(decay * 100.0)) + "%", vglib.rgba(160, 90, 255, 255));
            draw_knob("MIX", 450, 400, mix, string(vmath.round(mix * 100.0)) + "%", vglib.rgba(60, 220, 255, 255));
            draw_knob("PRE-DELAY", 650, 400, predelay / 100.0, string(vmath.round(predelay)) + "ms", vglib.rgba(255, 200, 50, 255));
            draw_knob("DAMPING", 850, 400, damping, string(vmath.round(damping * 100.0)) + "%", vglib.rgba(255, 90, 120, 255));

            draw_bypass_button(50, 810, rev_on, vglib.rgba(160, 90, 255, 255));
        }

        # --- GLOBAL MASTER STEREO OUTPUT METER (FAR RIGHT EDGE) ---
        vglib.rect(1150, 120, 20, 680, vglib.rgba(20, 24, 32, 255));
        m_rms = vmath.clamp(rms_val * 680.0, 0.0, 680.0);
        if (m_rms > 1.0) {
            vglib.rect(1150, (800) - m_rms, 20, m_rms, vglib.rgba(50, 255, 120, 255));
        }
        vglib.text_ex(vcr_font, "MASTER OUT", 1100, 820, 11, vglib.rgba(50, 255, 120, 255));

        # Footer
        vglib.text_ex(vcr_font, "Vyne Studio Engine v1.0.0", 900, 850, 12, vglib.rgba(150, 160, 180, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();