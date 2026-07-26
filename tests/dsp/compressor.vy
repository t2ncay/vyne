ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1000, 600, 60, "Vyne Opto Compressor Pro", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");
is_ready = vaudio.init_audio();
vaudio.volume(1.0);

track = vaudio.load_sound("tests/assets/fucking hardshit.wav");

vaudio.play_sound(track);
vaudio.attach_compressor(track);

run_time = 0.0;

thresh  :: Float64 = -18.0; # dB (-40.0 to 0.0)
ratio   :: Float64 = 4.0;   # 1.0 to 16.0
attack  :: Float64 = 15.0;  # ms (1.0 to 100.0)
release :: Float64 = 120.0; # ms (10.0 to 500.0)
makeup  :: Float64 = 3.0;   # dB (0.0 to 12.0)
enabled :: Int64   = 1;     # 1 = ON, 0 = BYPASS

active_knob = 0;

fn draw_knob(name, x, y, val_norm, display_val, color) {
    vglib.circle(x, y, 42.0, vglib.BLACK);
    vglib.circle(x, y, 39.0, vglib.rgba(45, 48, 58, 255));
    vglib.circle(x, y, 35.0, vglib.rgba(22, 24, 30, 255));
    vglib.circle(x, y, 22.0, vglib.rgba(32, 35, 44, 255)); # Inner Cap
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x :: Float64 = x + vmath.sin(rad) * 31.0;
    line_y :: Float64 = y - vmath.cos(rad) * 31.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 3.5, color);
    
    vglib.text_ex(vcr_font, name, x - 26, y + 48, 13, vglib.WHITE);
    vglib.text_ex(vcr_font, display_val, x - 20, y - 4, 11, color);
}

fn draw_bypass_button(x, y, is_active) {
    bg_color = vglib.rgba(35, 38, 48, 255);
    btn_color = vglib.rgba(220, 50, 50, 255);
    label = "BYPASS";

    if (is_active == 1) {
        btn_color = vglib.rgba(255, 120, 40, 255); # Hot Amber
        label = "ACTIVE";
    }

    vglib.rect(x, y, 110, 34, bg_color);
    vglib.rect(x + 2, y + 2, 106, 30, btn_color);
    vglib.text_ex(vcr_font, label, x + 18, y + 9, 13, vglib.BLACK);
}

# --- ANALOG VU METER FOR GAIN REDUCTION ---
fn draw_vu_meter(x, y, gr_db, is_on) {
    vglib.rect(x, y, 260, 180, vglib.rgba(14, 16, 20, 255));
    vglib.line(x, y, x + 260, y, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 260, y, x + 260, y + 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 260, y + 180, x, y + 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(x, y + 180, x, y, vglib.rgba(50, 55, 68, 255));

    face_col = is_on == 1 ? vglib.rgba(240, 230, 190, 255) : vglib.rgba(80, 80, 75, 255);
    vglib.rect(x + 12, y + 12, 236, 130, face_col);

    # Scale Tick Marks
    vglib.text_ex(vcr_font, "0", x + 30, y + 25, 11, vglib.BLACK);
    vglib.text_ex(vcr_font, "-6", x + 85, y + 20, 11, vglib.BLACK);
    vglib.text_ex(vcr_font, "-12", x + 140, y + 20, 11, vglib.BLACK);
    vglib.text_ex(vcr_font, "-24", x + 200, y + 25, 11, vglib.BLACK);

    pivot_x :: Float64 = x + 130.0;
    pivot_y :: Float64 = y + 135.0;

    norm_gr = vmath.clamp(gr_db / 24.0, 0.0, 1.0);
    needle_angle = -45.0 + (norm_gr * 90.0);
    rad = vmath.radians(needle_angle);

    needle_x :: Float64 = pivot_x + vmath.sin(rad) * 100.0;
    needle_y :: Float64 = pivot_y - vmath.cos(rad) * 100.0;

    vglib.line(pivot_x, pivot_y, needle_x, needle_y, vglib.rgba(200, 30, 30, 255));
    vglib.circle(pivot_x, pivot_y, 7.0, vglib.BLACK);

    vglib.text_ex(vcr_font, "GAIN REDUCTION", x + 75, y + 155, 11, vglib.rgba(160, 170, 185, 255));
}

# --- DYNAMIC KNEE GRAPH ---
fn draw_transfer_graph(x, y, t_dB, r_val, gr_db) {
    vglib.rect(x, y, 380, 180, vglib.rgba(14, 16, 20, 255));
    
    vglib.line(x, y, x + 380, y, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 380, y, x + 380, y + 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 380, y + 180, x, y + 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(x, y + 180, x, y, vglib.rgba(50, 55, 68, 255));

    vglib.line(x + 190, y, x + 190, y + 180, vglib.rgba(30, 36, 48, 255));
    vglib.line(x, y + 90, x + 380, y + 90, vglib.rgba(30, 36, 48, 255));
    
    vglib.line(x + 20, y + 160, x + 360, y + 20, vglib.rgba(55, 62, 75, 255));

    t_norm = (t_dB + 40.0) / 40.0;
    knee_x :: Float64 = x + 20.0 + (t_norm * 340.0);
    knee_y :: Float64 = y + 160.0 - (t_norm * 140.0);

    vglib.line(x + 20.0, y + 160.0, knee_x, knee_y, vglib.rgba(255, 140, 40, 255));

    end_x :: Float64 = x + 360.0;
    compressed_rise = (140.0 * (1.0 - t_norm)) / r_val;
    end_y :: Float64 = knee_y - compressed_rise;

    vglib.line(knee_x, knee_y, end_x, end_y, vglib.rgba(255, 140, 40, 255));
    vglib.circle(knee_x, knee_y, 4.0, vglib.WHITE);

    if (gr_db > 0.5) {
        gr_norm = vmath.clamp(gr_db / 24.0, 0.0, 1.0);
        dot_x :: Float64 = knee_x + (gr_norm * (end_x - knee_x));
        dot_y :: Float64 = knee_y - (gr_norm * (knee_y - end_y));
        vglib.circle(dot_x, dot_y, 6.0, vglib.rgba(255, 60, 60, 255));
    }

    vglib.text_ex(vcr_font, "TRANSFER FUNCTION & DYNAMICS KNEE", x + 50, y + 190, 11, vglib.rgba(160, 170, 185, 255));
}

# --- PRO OUTPUT RMS METER CARD ---
fn draw_output_rms_card(x, y, rms_val) {
    vglib.rect(x, y, 140, 180, vglib.rgba(14, 16, 20, 255));
    
    vglib.line(x, y, x + 140, y, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 140, y, x + 140, y + 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(x + 140, y + 180, x, y + 180, vglib.rgba(50, 55, 68, 255));
    vglib.line(x, y + 180, x, y, vglib.rgba(50, 55, 68, 255));

    # dB Reference Lines
    vglib.text_ex(vcr_font, " 0dB", x + 10, y + 20, 9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "-12dB", x + 10, y + 55, 9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "-24dB", x + 10, y + 90, 9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "-48dB", x + 10, y + 130, 9, vglib.rgba(140, 150, 165, 255));

    # Dual Stereo Bars
    vglib.rect(x + 65, y + 15, 22, 135, vglib.rgba(24, 28, 36, 255));
    vglib.rect(x + 95, y + 15, 22, 135, vglib.rgba(24, 28, 36, 255));

    rms_h = vmath.clamp(rms_val * 135.0, 0.0, 135.0);
    if (rms_h > 1.0) {
        # Active Green Glow Bars
        vglib.rect(x + 65, (y + 150) - rms_h, 22, rms_h, vglib.rgba(50, 255, 120, 255));
        vglib.rect(x + 95, (y + 150) - rms_h, 22, rms_h, vglib.rgba(50, 255, 120, 255));
    }

    vglib.text_ex(vcr_font, "OUTPUT RMS", x + 35, y + 158, 11, vglib.rgba(50, 255, 120, 255));
}

while (vglib.running()) {
    run_time = run_time + 0.016;
    
    if (vaudio.is_playing(track) == 0) {
        vaudio.play_sound(track);
    }
    
    m = vglib.mouse_pos();
    md = vglib.mouse_delta();

    # --- KNOB DRAGGING INTERACTION (Well-spaced bottom row) ---
    if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
        if (active_knob == 0) {
            if (vmath.hypot(m[0] - 120, m[1] - 420) < 45) { active_knob = 1; } # Thresh
            if (vmath.hypot(m[0] - 280, m[1] - 420) < 45) { active_knob = 2; } # Ratio
            if (vmath.hypot(m[0] - 440, m[1] - 420) < 45) { active_knob = 3; } # Attack
            if (vmath.hypot(m[0] - 600, m[1] - 420) < 45) { active_knob = 4; } # Release
            if (vmath.hypot(m[0] - 760, m[1] - 420) < 45) { active_knob = 5; } # Makeup
        }

        delta = md[1] * 0.3;
        if (active_knob == 1) { thresh = vmath.clamp(thresh - delta, -40.0, 0.0); }
        if (active_knob == 2) { ratio  = vmath.clamp(ratio - (delta * 0.1), 1.0, 16.0); }
        if (active_knob == 3) { attack = vmath.clamp(attack - delta, 1.0, 100.0); }
        if (active_knob == 4) { release = vmath.clamp(release - (delta * 2.0), 10.0, 500.0); }
        if (active_knob == 5) { makeup = vmath.clamp(makeup - (delta * 0.1), 0.0, 12.0); }
    } else {
        active_knob = 0;
    }

    # TOGGLE BYPASS (SPACEBAR)
    if (vglib.key_pressed(vglib.SPACE)) {
        if (enabled == 1) { enabled = 0; } else { enabled = 1; }
    }

    # TOGGLE BYPASS (MOUSE CLICK)
    if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
        if (m[0] >= 50 && m[0] <= 160 && m[1] >= 530 && m[1] <= 564) {
            if (enabled == 1) { enabled = 0; } else { enabled = 1; }
        }
    }

    vaudio.set_compressor(thresh, ratio, attack, release, makeup, enabled);

    gr_db   = vaudio.get_gr();   # Gain Reduction (dB)
    rms_val = vaudio.get_rms();  # RMS Output Level (0.0 to 1.0)

    vglib.begin();
        vglib.clear(vglib.rgba(14, 16, 20, 255)); # Studio Dark Chassis

        # Top Accent Header Bar
        header_color = (enabled == 1) ? vglib.rgba(255, 120, 40, 180) : vglib.rgba(80, 80, 90, 180);
        vglib.rect(50, 45, 900, 3, header_color);
        vglib.text_ex(vcr_font, "VYNE OPTO COMPRESSOR PRO", 300, 65, 24, vglib.WHITE);
        vglib.text_ex(vcr_font, "Precision Peak & Dynamic Bus Limiter", 325, 95, 12, vglib.rgba(200, 200, 210, 255));

        # --- TOP DISPLAY RACK (3 FULLY BALANCED CARDS: GRAPH + VU + RMS) ---
        draw_transfer_graph(50, 130, thresh, ratio, gr_db);
        draw_vu_meter(450, 130, gr_db, enabled);
        draw_output_rms_card(730, 130, rms_val);

        # --- PARAMETER KNOBS (PERFECTLY CENTERED BOTTOM ROW) ---
        t_norm   = (thresh + 40.0) / 40.0;
        r_norm   = (ratio - 1.0) / 15.0;
        a_norm   = (attack - 1.0) / 99.0;
        rel_norm = (release - 10.0) / 490.0;
        m_norm   = makeup / 12.0;

        orange_glow = (enabled == 1) ? vglib.rgba(255, 130, 40, 255) : vglib.rgba(90, 90, 100, 255);

        draw_knob("THRESH", 120, 410, t_norm, string(vmath.round(thresh)) + "dB", orange_glow);
        draw_knob("RATIO", 280, 410, r_norm, string(vmath.round(ratio)) + ":1", vglib.rgba(50, 200, 255, 255));
        draw_knob("ATTACK", 440, 410, a_norm, string(vmath.round(attack)) + "ms", vglib.rgba(255, 220, 50, 255));
        draw_knob("RELEASE", 600, 410, rel_norm, string(vmath.round(release)) + "ms", vglib.rgba(180, 100, 255, 255));
        draw_knob("MAKEUP", 760, 410, m_norm, "+" + string(vmath.round(makeup)) + "dB", vglib.rgba(50, 255, 120, 255));

        # --- BYPASS BUTTON & FOOTER ---
        draw_bypass_button(50, 530, enabled);
        vglib.text_ex(vcr_font, "Vyne Studio Suite v0.0.1", 720, 545, 12, vglib.rgba(255, 100, 50, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();