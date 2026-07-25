ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1000, 600, 60, "Vyne Opto Compressor", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");
is_ready = vaudio.init_audio();
out("Audio Device Ready: " + string(is_ready));
vaudio.volume(1.0);

track = vaudio.load_sound("tests/assets/fucking hardshit.wav");
out("Track Pointer Handle : " + string(track));

vaudio.play_sound(track);
vaudio.attach_compressor(track);

run_time = 0.0;

thresh  :: Float64 = -18.0; # dB (-40.0 to 0.0)
ratio   :: Float64 = 4.0;   # 1.0 to 16.0
attack  :: Float64 = 10.0;  # ms (1.0 to 100.0)
release :: Float64 = 120.0; # ms (10.0 to 500.0)
makeup  :: Float64 = 4.0;   # dB (0.0 to 12.0)
enabled :: Int64   = 1;     # 1 = ON, 0 = BYPASS

active_knob = 0;

fn draw_knob(name, x, y, val_norm, display_val, color) {
    vglib.circle(x, y, 42.0, vglib.BLACK);
    vglib.circle(x, y, 40.0, vglib.rgba(50, 52, 62, 255));
    vglib.circle(x, y, 36.0, vglib.rgba(24, 26, 32, 255));
    vglib.circle(x, y, 22.0, vglib.rgba(35, 38, 46, 255)); # Inner Cap
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x = x + vmath.sin(rad) * 32.0;
    line_y = y - vmath.cos(rad) * 32.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 4.0, color);
    
    # Labels
    vglib.text_ex(vcr_font, name, x - 28, y + 50, 14, vglib.WHITE);
    vglib.text_ex(vcr_font, display_val, x - 22, y - 5, 12, color);
}

fn draw_bypass_button(x, y, is_active) {
    bg_color = vglib.rgba(40, 40, 50, 255);
    btn_color = vglib.rgba(220, 50, 50, 255);
    label = "BYPASS";

    if (is_active == 1) {
        btn_color = vglib.rgba(255, 120, 40, 255);
        label = "ACTIVE";
    }

    vglib.rect(x, y, 110, 38, bg_color);
    vglib.rect(x + 2, y + 2, 106, 34, btn_color);
    vglib.text_ex(vcr_font, label, x + 16, y + 11, 14, vglib.BLACK);
}

fn draw_transfer_graph(x, y, t_dB, r_val) {
    vglib.rect(x, y, 110, 110, vglib.rgba(12, 14, 18, 255));
    
    vglib.line(x, y, x + 110, y, vglib.rgba(50, 55, 65, 255));
    vglib.line(x + 110, y, x + 110, y + 110, vglib.rgba(50, 55, 65, 255));
    vglib.line(x + 110, y + 110, x, y + 110, vglib.rgba(50, 55, 65, 255));
    vglib.line(x, y + 110, x, y, vglib.rgba(50, 55, 65, 255));
    
    vglib.line(x + 55, y, x + 55, y + 110, vglib.rgba(30, 35, 45, 255));
    vglib.line(x, y + 55, x + 110, y + 55, vglib.rgba(30, 35, 45, 255));
    
    vglib.line(x + 5, y + 105, x + 105, y + 5, vglib.rgba(60, 65, 75, 255));

    t_norm = (t_dB + 40.0) / 40.0;
    knee_x = x + 5 + (t_norm * 100.0);
    knee_y = y + 105 - (t_norm * 100.0);

    vglib.line(x + 5, y + 105, knee_x, knee_y, vglib.rgba(255, 140, 40, 255));

    end_x = x + 105;
    compressed_rise = (100.0 * (1.0 - t_norm)) / r_val;
    end_y = knee_y - compressed_rise;

    vglib.line(knee_x, knee_y, end_x, end_y, vglib.rgba(255, 140, 40, 255));
    vglib.circle(knee_x, knee_y, 3.0, vglib.WHITE);
    
    vglib.text_ex(vcr_font, "KNEE", x + 35, y + 115, 10, vglib.rgba(180, 180, 190, 255));
}

while (vglib.running()) {
    run_time = run_time + 0.016;
    
    if (vaudio.is_playing(track) == 0) {
        vaudio.play_sound(track);
    }
    
    m = vglib.mouse_pos();
    md = vglib.mouse_delta();

    if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
        if (active_knob == 0) {
            if (vmath.hypot(m[0] - 120, m[1] - 320) < 45) { active_knob = 1; } # Thresh
            if (vmath.hypot(m[0] - 260, m[1] - 320) < 45) { active_knob = 2; } # Ratio
            if (vmath.hypot(m[0] - 400, m[1] - 320) < 45) { active_knob = 3; } # Attack
            if (vmath.hypot(m[0] - 540, m[1] - 320) < 45) { active_knob = 4; } # Release
            if (vmath.hypot(m[0] - 680, m[1] - 320) < 45) { active_knob = 5; } # Makeup
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
        if (m[0] >= 50 && m[0] <= 160 && m[1] >= 520 && m[1] <= 558) {
            if (enabled == 1) { enabled = 0; } else { enabled = 1; }
        }
    }

    vaudio.set_compressor(thresh, ratio, attack, release, makeup, enabled);

    gr_db   = vaudio.get_gr();   # Gain Reduction (dB)
    rms_val = vaudio.get_rms();  # RMS Output Level (0.0 to 1.0)

    vglib.begin();
        vglib.clear(vglib.rgba(16, 18, 22, 255)); # Studio Chassis Dark Grey

        # --- 1. GAIN REDUCTION (GR) METER (RED, DOWNWARDS) ---
        vglib.rect(830, 200, 22, 200, vglib.rgba(30, 32, 40, 255));
        meter_h = vmath.clamp((gr_db / 24.0) * 200.0, 0.0, 200.0);
        
        if (meter_h > 1.0 && enabled == 1) {
            vglib.rect(830, 200, 22, meter_h, vglib.rgba(255, 60, 70, 255));
        }

        vglib.text_ex(vcr_font, "GR", 830, 175, 14, vglib.WHITE);
        vglib.text_ex(vcr_font, "-" + string(vmath.round(gr_db)) + "dB", 820, 410, 12, vglib.rgba(255, 100, 100, 255));

        # --- 2. RMS OUTPUT METER (GREEN, UPWARDS) ---
        vglib.rect(880, 200, 22, 200, vglib.rgba(30, 32, 40, 255));
        rms_h = vmath.clamp(rms_val * 200.0, 0.0, 200.0);
        
        if (rms_h > 1.0) {
            vglib.rect(880, 400 - rms_h, 22, rms_h, vglib.rgba(50, 255, 120, 255));
        }

        vglib.text_ex(vcr_font, "RMS", 878, 175, 14, vglib.WHITE);
        vglib.text_ex(vcr_font, string(vmath.round(rms_val * 100)) + "%", 875, 410, 12, vglib.rgba(50, 255, 120, 255));

        # --- 3. KNOB NORMALIZE & COLOR STATES ---
        t_norm   = (thresh + 40.0) / 40.0;
        r_norm   = (ratio - 1.0) / 15.0;
        a_norm   = (attack - 1.0) / 99.0;
        rel_norm = (release - 10.0) / 490.0;
        m_norm   = makeup / 12.0;

        orange_glow = vglib.rgba(255, 130, 40, 255);
        if (enabled == 0) { orange_glow = vglib.rgba(90, 90, 100, 255); }

        draw_knob("THRESH", 120, 320, t_norm, string(vmath.round(thresh)) + "dB", orange_glow);
        draw_knob("RATIO", 260, 320, r_norm, string(vmath.round(ratio)) + ":1", vglib.rgba(50, 200, 255, 255));
        draw_knob("ATTACK", 400, 320, a_norm, string(vmath.round(attack)) + "ms", vglib.rgba(255, 220, 50, 255));
        draw_knob("RELEASE", 540, 320, rel_norm, string(vmath.round(release)) + "ms", vglib.rgba(180, 100, 255, 255));
        draw_knob("MAKEUP", 680, 320, m_norm, "+" + string(vmath.round(makeup)) + "dB", vglib.rgba(50, 255, 120, 255));
        
        draw_transfer_graph(45, 130, thresh, ratio);

        draw_bypass_button(50, 520, enabled);

        header_color = vglib.rgba(255, 120, 40, 180);
        if (enabled == 0) { header_color = vglib.rgba(80, 80, 90, 180); }

        vglib.rect(50, 55, 900, 3, header_color); # Top Accent Bar
        vglib.text_ex(vcr_font, "VYNE OPTO COMPRESSOR", 320, 85, 24, vglib.WHITE);
        vglib.text_ex(vcr_font, "Precision Peak & RMS Bus Dynamics", 335, 115, 12, vglib.rgba(200, 200, 210, 255));
        vglib.text_ex(vcr_font, "Vyne Studio Rack v0.0.3", 700, 550, 12, vglib.rgba(255, 100, 50, 255));
        
    vglib.end();
}

vaudio.close_audio();
vglib.close();