ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1000, 600, 60, "Vyne Spatial Reverb Pro", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

is_ready = vaudio.init_audio();
out("Audio Device Ready: " + string(is_ready));
vaudio.volume(1.0);

track = vaudio.load_sound("tests/assets/fucking hardshit.wav");
out("Track Pointer Handle : " + string(track));

vaudio.play_sound(track);
vaudio.attach_reverb(track);

run_time = 0.0;

# --- REVERB DSP PARAMETERS ---
decay    :: Float64 = 0.85; # Room Decay Rate (0.0 to 0.95)
mix      :: Float64 = 0.45; # Wet/Dry Mix (0.0 to 1.0)
predelay :: Float64 = 20.0; # Pre-delay in ms (0.0 to 100.0)
damping  :: Float64 = 0.30; # High-Frequency Damping (0.0 to 1.0)
enabled  :: Int64   = 1;    # 1 = ACTIVE, 0 = BYPASS

active_knob = 0;

fn draw_knob(name, x, y, val_norm, display_val, color) {
    vglib.circle(x, y, 42.0, vglib.BLACK);
    vglib.circle(x, y, 40.0, vglib.rgba(45, 48, 58, 255));
    vglib.circle(x, y, 36.0, vglib.rgba(25, 27, 34, 255));
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x = x + vmath.sin(rad) * 30.0;
    line_y = y - vmath.cos(rad) * 30.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 4.0, color);
    
    # Knob Labels
    vglib.text_ex(vcr_font, name, x - 28, y + 52, 14, vglib.WHITE);
    vglib.text_ex(vcr_font, display_val, x - 22, y - 5, 12, color);
}

fn draw_bypass_button(x, y, is_active) {
    bg_color = vglib.rgba(40, 40, 50, 255);
    btn_color = vglib.rgba(220, 50, 50, 255); # BYPASS (Red)
    label = "BYPASS";

    if (is_active == 1) {
        btn_color = vglib.rgba(140, 80, 255, 255); # ACTIVE (Purple Ambient Reverb Glow)
        label = "ACTIVE";
    }

    vglib.rect(x, y, 110, 38, bg_color);
    vglib.rect(x + 2, y + 2, 106, 34, btn_color);
    vglib.text_ex(vcr_font, label, x + 16, y + 11, 14, vglib.BLACK);
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
            if (vmath.hypot(m[0] - 200, m[1] - 300) < 45) { active_knob = 1; } # Decay
            if (vmath.hypot(m[0] - 360, m[1] - 300) < 45) { active_knob = 2; } # Mix
            if (vmath.hypot(m[0] - 520, m[1] - 300) < 45) { active_knob = 3; } # Pre-delay
            if (vmath.hypot(m[0] - 680, m[1] - 300) < 45) { active_knob = 4; } # Damping
        }

        delta = md[1] * 0.3;
        if (active_knob == 1) { decay    = vmath.clamp(decay - (delta * 0.01), 0.0, 0.95); }
        if (active_knob == 2) { mix      = vmath.clamp(mix - (delta * 0.01), 0.0, 1.0); }
        if (active_knob == 3) { predelay = vmath.clamp(predelay - delta, 0.0, 100.0); }
        if (active_knob == 4) { damping  = vmath.clamp(damping - (delta * 0.01), 0.0, 1.0); }
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

    vaudio.set_reverb(decay, mix, enabled);

    rms_val = vaudio.get_rms();

    vglib.begin();
        # Dark Space Aesthetic
        vglib.clear(vglib.rgba(16, 18, 24, 255));

        vglib.rect(840, 200, 18, 200, vglib.rgba(30, 32, 42, 255));
        l_meter_h = vmath.clamp(rms_val * 200.0, 0.0, 200.0);
        if (l_meter_h > 1.0) {
            vglib.rect(840, 400 - l_meter_h, 18, l_meter_h, vglib.rgba(160, 90, 255, 255));
        }
        vglib.text_ex(vcr_font, "L", 845, 175, 14, vglib.WHITE);

        vglib.rect(880, 200, 18, 200, vglib.rgba(30, 32, 42, 255));
        r_meter_h = vmath.clamp(rms_val * 190.0, 0.0, 200.0); # Subtle offset for visual stereo
        if (r_meter_h > 1.0) {
            vglib.rect(880, 400 - r_meter_h, 18, r_meter_h, vglib.rgba(60, 220, 255, 255));
        }
        vglib.text_ex(vcr_font, "R", 885, 175, 14, vglib.WHITE);

        vglib.text_ex(vcr_font, string(vmath.round(rms_val * 100)) + "%", 842, 410, 12, vglib.rgba(160, 120, 255, 255));

        decay_norm   = decay / 0.95;
        mix_norm     = mix;
        predelay_norm = predelay / 100.0;
        damp_norm    = damping;

        decay_color = vglib.rgba(160, 90, 255, 255); # Purple
        mix_color   = vglib.rgba(60, 220, 255, 255); # Cyan
        
        if (enabled == 0) { 
            decay_color = vglib.rgba(90, 90, 100, 255); 
            mix_color   = vglib.rgba(90, 90, 100, 255); 
        }

        draw_knob("DECAY", 200, 300, decay_norm, string(vmath.round(decay * 100.0)) + "%", decay_color);
        draw_knob("MIX", 360, 300, mix_norm, string(vmath.round(mix * 100.0)) + "%", mix_color);
        draw_knob("PRE-DELAY", 520, 300, predelay_norm, string(vmath.round(predelay)) + "ms", vglib.rgba(255, 200, 50, 255));
        draw_knob("DAMPING", 680, 300, damp_norm, string(vmath.round(damping * 100.0)) + "%", vglib.rgba(255, 90, 120, 255));
        
        draw_bypass_button(50, 520, enabled);

        vglib.rect(50, 60, 900, 3, vglib.rgba(140, 80, 255, 180)); # Metallic Accent Line
        vglib.text_ex(vcr_font, "VYNE SPATIAL REVERB", 310, 90, 24, vglib.WHITE);
        vglib.text_ex(vcr_font, "Shoegaze Atmospheric Processor v0.0.1", 285, 120, 12, vglib.rgba(160, 120, 255, 255));
        vglib.text_ex(vcr_font, "Vyne Studio Rack", 710, 550, 12, vglib.rgba(140, 80, 255, 255));
        
    vglib.end();
}

vaudio.close_audio();
vglib.close();