ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1000, 600, 60, "Vyne Saturator Pro", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");
is_ready = vaudio.init_audio();
vaudio.volume(1.0);

track = vaudio.load_sound("tests/assets/fucking hardshit.wav");

# Start playback and attach saturator processor ONCE
vaudio.play_sound(track);
vaudio.attach_saturator(track);

run_time = 0.0;
drive    :: Float64 = 0.45;
mode     :: Int64   = 0; # 0 = SOFT TUBE, 1 = HARD CLIP, 2 = ASYMMETRIC

active_knob = 0;

fn draw_knob(name, x, y, val_norm, display_val, color) {
    vglib.circle(x, y, 48.0, vglib.BLACK);
    vglib.circle(x, y, 45.0, vglib.rgba(50, 52, 62, 255));
    vglib.circle(x, y, 40.0, vglib.rgba(24, 26, 32, 255));
    vglib.circle(x, y, 26.0, vglib.rgba(35, 38, 46, 255));
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x = x + vmath.sin(rad) * 36.0;
    line_y = y - vmath.cos(rad) * 36.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 4.0, color);
    
    vglib.text_ex(vcr_font, name, x - 26, y + 58, 14, vglib.WHITE);
    vglib.text_ex(vcr_font, display_val, x - 20, y - 5, 12, color);
}

fn draw_transfer_curve(x, y, d_val, m_val) {
    # Box Background
    vglib.rect(x, y, 140, 140, vglib.rgba(14, 16, 20, 255));
    
    # Outer Border (Using 4 lines)
    vglib.line(x, y, x + 140, y, vglib.rgba(50, 55, 65, 255));
    vglib.line(x + 140, y, x + 140, y + 140, vglib.rgba(50, 55, 65, 255));
    vglib.line(x + 140, y + 140, x, y + 140, vglib.rgba(50, 55, 65, 255));
    vglib.line(x, y + 140, x, y, vglib.rgba(50, 55, 65, 255));

    # Grid Center Lines
    vglib.line(x + 70, y, x + 70, y + 140, vglib.rgba(32, 38, 48, 255));
    vglib.line(x, y + 70, x + 140, y + 70, vglib.rgba(32, 38, 48, 255));

    # Linear Reference Line (Clean 1:1)
    vglib.line(x + 10, y + 130, x + 130, y + 10, vglib.rgba(50, 55, 65, 255));

    # Explicit Float64 typing for graph coordinates
    prev_px :: Float64 = x + 10.0;
    prev_py :: Float64 = y + 70.0;
    gain = 1.0 + (d_val * 7.0);

    step = 4.0;
    curr_px :: Float64 = x + 10.0;

    while (curr_px <= x + 130.0) {
        norm_in = ((curr_px - (x + 10.0)) / 120.0 * 2.0) - 1.0;
        sample = norm_in * gain;

        if (m_val == 0) {
            sample = vmath.tanh(sample);
        }
        if (m_val == 1) {
            sample = vmath.clamp(sample, -0.7, 0.7) * 1.42;
        }
        if (m_val == 2) {
            if (sample > 0.0) { sample = vmath.tanh(sample); }
            else { sample = vmath.tanh(sample * 1.5) * 0.8; }
        }

        norm_out = vmath.clamp(sample * (1.0 / vmath.sqrt(gain)), -1.0, 1.0);
        curr_py :: Float64 = (y + 70.0) - (norm_out * 60.0);

        curve_color = vglib.rgba(255, 100, 40, 255);

        if (curr_px > x + 10.0) {
            vglib.line(prev_px, prev_py, curr_px, curr_py, curve_color);
        }

        prev_px = curr_px;
        prev_py = curr_py;
        curr_px = curr_px + step;
    }

    vglib.text_ex(vcr_font, "TRANSFER CURVE", x + 15, y + 148, 10, vglib.rgba(160, 170, 185, 255));
}

while (vglib.running()) {
    run_time = run_time + 0.016;
    
    # Auto-loop track when finished
    if (vaudio.is_playing(track) == false) {
        vaudio.play_sound(track);
    }
    
    m = vglib.mouse_pos();
    md = vglib.mouse_delta();

    # --- DRIVE KNOB INTERACTION ---
    if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
        if (active_knob == 0) {
            if (vmath.hypot(m[0] - 220, m[1] - 300) < 55) { active_knob = 1; }
        }

        delta = md[1] * 0.3;
        if (active_knob == 1) { drive = vmath.clamp(drive - (delta * 0.01), 0.0, 1.0); }
    } else {
        active_knob = 0;
    }

    # --- MODE SELECTOR CLICK INTERACTION ---
    if (vglib.key_pressed(vglib.MOUSE_LEFT)) {
        if (m[0] >= 420 && m[0] <= 560 && m[1] >= 220 && m[1] <= 255) { mode = 0; }
        if (m[0] >= 420 && m[0] <= 560 && m[1] >= 270 && m[1] <= 305) { mode = 1; }
        if (m[0] >= 420 && m[0] <= 560 && m[1] >= 320 && m[1] <= 355) { mode = 2; }
    }

    # Spacebar Cycle Mode
    if (vglib.key_pressed(vglib.SPACE)) { 
        mode = (mode + 1) % 3; 
    }

    # Update C++ DSP Parameters
    vaudio.set_dsp(drive, mode);

    rms_val = vaudio.get_rms();

    vglib.begin();
        vglib.clear(vglib.rgba(16, 18, 22, 255)); # Studio Dark Frame

        # Header Accent Line
        vglib.rect(50, 55, 900, 3, vglib.rgba(255, 100, 40, 180));
        vglib.text_ex(vcr_font, "VYNE SATURATOR", 360, 85, 24, vglib.WHITE);
        vglib.text_ex(vcr_font, "Analog Tube & Non-Linear Harmonics", 325, 115, 12, vglib.rgba(200, 200, 210, 255));

        # --- DRIVE KNOB ---
        orange_glow = vglib.rgba(255, 100, 40, 255);
        draw_knob("DRIVE", 220, 300, drive, string(vmath.round(drive * 100.0)) + "%", orange_glow);

        # --- MODE SELECTOR RACK CARD ---
        vglib.rect(400, 180, 180, 195, vglib.rgba(22, 26, 34, 255));
        vglib.text_ex(vcr_font, "SATURATION MODE", 415, 195, 12, vglib.WHITE);

        btn0_col = (mode == 0) ? vglib.rgba(255, 100, 40, 255) : vglib.rgba(40, 45, 58, 255);
        btn1_col = (mode == 1) ? vglib.rgba(255, 100, 40, 255) : vglib.rgba(40, 45, 58, 255);
        btn2_col = (mode == 2) ? vglib.rgba(255, 100, 40, 255) : vglib.rgba(40, 45, 58, 255);

        vglib.rect(420, 220, 140, 35, btn0_col);
        vglib.text_ex(vcr_font, "SOFT TUBE", 438, 232, 12, (mode == 0) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(420, 270, 140, 35, btn1_col);
        vglib.text_ex(vcr_font, "HARD CLIP", 438, 282, 12, (mode == 1) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(420, 320, 140, 35, btn2_col);
        vglib.text_ex(vcr_font, "ASYMMETRIC", 430, 332, 12, (mode == 2) ? vglib.BLACK : vglib.WHITE);

        # --- LIVE TRANSFER CURVE GRAPH ---
        draw_transfer_curve(640, 210, drive, mode);

        # --- WAVEFORM VISUALIZER BARS ---
        vglib.rect(50, 430, 900, 50, vglib.rgba(22, 26, 34, 255));
        
        through i :: 0..88 -> loop {
            x_p = 60 + (i * 10);
            wave_in = vmath.sin(run_time * 8.0 + i * 0.15);
            
            if (mode == 0) { wave_in = vmath.tanh(wave_in * (1.0 + drive * 5.0)); }
            if (mode == 1) { wave_in = vmath.clamp(wave_in * (1.0 + drive * 5.0), -0.7, 0.7); }
            if (mode == 2) {
                if (wave_in > 0.0) { wave_in = vmath.tanh(wave_in * (1.0 + drive * 5.0)); }
                else { wave_in = vmath.tanh(wave_in * (1.0 + drive * 7.5)) * 0.8; }
            }
            
            h = vmath.abs(wave_in) * 20.0;
            vglib.rect(x_p, 455 - h, 6, h * 2.0, vglib.rgba(255, 110, 40, 255));
        };

        # --- RMS OUTPUT LEVEL METER ---
        vglib.rect(880, 180, 22, 195, vglib.rgba(22, 26, 34, 255));
        rms_h = vmath.clamp(rms_val * 195.0, 0.0, 195.0);
        if (rms_h > 1.0) {
            vglib.rect(880, 375 - rms_h, 22, rms_h, vglib.rgba(255, 120, 40, 255));
        }
        vglib.text_ex(vcr_font, "OUT", 878, 382, 10, vglib.rgba(255, 120, 40, 255));

        # Footer Title
        vglib.text_ex(vcr_font, "Vyne Audio Suite v0.0.1", 720, 550, 12, vglib.rgba(255, 100, 40, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();