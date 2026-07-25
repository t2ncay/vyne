ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1000, 600, 60, "Vyne Dynamic Compressor Pro", 0);
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
    vglib.circle(x, y, 38.0, vglib.BLACK);
    vglib.circle(x, y, 36.0, vglib.rgba(60, 60, 70, 255));
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x = x + vmath.sin(rad) * 32.0;
    line_y = y - vmath.cos(rad) * 32.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 4.0, color);
    
    vglib.text_ex(vcr_font, name, x - 28, y + 48, 14, vglib.WHITE);
    vglib.text_ex(vcr_font, display_val, x - 20, y - 5, 12, color);
}

fn draw_bypass_button(x, y, is_active) {
    bg_color = vglib.rgba(40, 40, 50, 255);
    btn_color = vglib.rgba(220, 50, 50, 255); # BYPASS (Red)
    label = "BYPASS";

    if (is_active == 1) {
        btn_color = vglib.rgba(50, 220, 100, 255); # ACTIVE (Green)
        label = "ACTIVE";
    }

    vglib.rect(x, y, 110, 38, bg_color);
    vglib.rect(x + 2, y + 2, 106, 34, btn_color);
    vglib.text_ex(vcr_font, label, x + 16, y + 11, 14, vglib.BLACK);
}

while (vglib.running()) {
    run_time = run_time + 0.016;
    
    # Auto-loop track when finished
    if (vaudio.is_playing(track) == 0) {
        vaudio.play_sound(track);
    }
    
    m = vglib.mouse_pos();
    md = vglib.mouse_delta();

    if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
        if (active_knob == 0) {
            if (vmath.hypot(m[0] - 150, m[1] - 300) < 45) { active_knob = 1; }
            if (vmath.hypot(m[0] - 300, m[1] - 300) < 45) { active_knob = 2; }
            if (vmath.hypot(m[0] - 450, m[1] - 300) < 45) { active_knob = 3; }
            if (vmath.hypot(m[0] - 600, m[1] - 300) < 45) { active_knob = 4; }
            if (vmath.hypot(m[0] - 750, m[1] - 300) < 45) { active_knob = 5; }
        }

        delta = md[1] * 0.3;
        if (active_knob == 1) { thresh = vmath.clamp(thresh - delta, -40.0, 0.0); }
        if (active_knob == 2) { ratio = vmath.clamp(ratio - (delta * 0.1), 1.0, 16.0); }
        if (active_knob == 3) { attack = vmath.clamp(attack - delta, 1.0, 100.0); }
        if (active_knob == 4) { release = vmath.clamp(release - (delta * 2.0), 10.0, 500.0); }
        if (active_knob == 5) { makeup = vmath.clamp(makeup - (delta * 0.1), 0.0, 12.0); }
    } else {
        active_knob = 0;
    }

    if (vglib.key_pressed(vglib.SPACE)) {
        if (enabled == 1) { enabled = 0; } else { enabled = 1; }
    }

    if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
        if (m[0] >= 50 && m[0] <= 160 && m[1] >= 520 && m[1] <= 558) {
            if (enabled == 1) { enabled = 0; } else { enabled = 1; }
        }
    }

    vaudio.set_compressor(thresh, ratio, attack, release, makeup, enabled);

    gr_db   = vaudio.get_gr();   # Gain Reduction (dB)
    rms_val = vaudio.get_rms();  # RMS Output Level (0.0 to 1.0)

    vglib.begin();
        vglib.clear(vglib.rgba(20, 22, 28, 255));

        vglib.rect(850, 200, 20, 200, vglib.rgba(40, 40, 50, 255));
        meter_h = vmath.clamp((gr_db / 24.0) * 200.0, 0.0, 200.0);
        
        if (meter_h > 1.0 && enabled == 1) {
            vglib.rect(850, 200, 20, meter_h, vglib.rgba(255, 60, 60, 255));
        }

        vglib.text_ex(vcr_font, "GR", 850, 175, 14, vglib.WHITE);
        vglib.text_ex(vcr_font, "-" + string(vmath.round(gr_db)) + "dB", 840, 410, 12, vglib.rgba(255, 100, 100, 255));

        vglib.rect(900, 200, 20, 200, vglib.rgba(40, 40, 50, 255));
        rms_h = vmath.clamp(rms_val * 200.0, 0.0, 200.0);
        
        if (rms_h > 1.0) {
            vglib.rect(900, 400 - rms_h, 20, rms_h, vglib.rgba(50, 255, 120, 255));
        }

        vglib.text_ex(vcr_font, "RMS", 898, 175, 14, vglib.WHITE);
        vglib.text_ex(vcr_font, string(vmath.round(rms_val * 100)) + "%", 895, 410, 12, vglib.rgba(50, 255, 120, 255));

        t_norm   = (thresh + 40.0) / 40.0;
        r_norm   = (ratio - 1.0) / 15.0;
        a_norm   = (attack - 1.0) / 99.0;
        rel_norm = (release - 10.0) / 490.0;
        m_norm   = makeup / 12.0;

        active_color = vglib.rgba(255, 120, 50, 255);
        if (enabled == 0) { active_color = vglib.rgba(100, 100, 110, 255); }

        draw_knob("THRESH", 150, 300, t_norm, string(vmath.round(thresh)) + "dB", active_color);
        draw_knob("RATIO", 300, 300, r_norm, string(vmath.round(ratio)) + ":1", vglib.rgba(50, 200, 255, 255));
        draw_knob("ATTACK", 450, 300, a_norm, string(vmath.round(attack)) + "ms", vglib.rgba(255, 220, 50, 255));
        draw_knob("RELEASE", 600, 300, rel_norm, string(vmath.round(release)) + "ms", vglib.rgba(180, 100, 255, 255));
        draw_knob("MAKEUP", 750, 300, m_norm, "+" + string(vmath.round(makeup)) + "dB", vglib.rgba(50, 255, 120, 255));
        
        draw_bypass_button(50, 520, enabled);

        # Header Info
        vglib.text_ex(vcr_font, "VYNE DYNAMIC COMPRESSOR", 320, 100, 24, vglib.WHITE);
        vglib.text_ex(vcr_font, "Vyne Studio Rack v0.0.2", 710, 550, 12, vglib.RED);
        
    vglib.end();
}

vaudio.close_audio();
vglib.close();