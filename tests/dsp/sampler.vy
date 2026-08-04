ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

use "configs/config.vy";

vglib.init(1400, 900, 60, "VYNE SERATO SAMPLER CONSOLE v1.0", 0);
vcr_font = vglib.load_font(configs.Fonts.vcr_mono);

is_ready = vaudio.init_audio();
vaudio.volume(1.0);

track_name :: String = "tests/assetts/cigerlerim.mp3";

# 1. SWITCH LOAD TO PLAY_STREAM FOR AUDIO STREAMING
track = vaudio.play_stream(configs.Audios.osamason_1300);

run_time = 0.0;
prev_mouse_state = 0;

# --- SAMPLER ENGINE STATE ---
pitch_shift  :: Float64 = 0.0;   # -12 to +12 semitones
speed_rate   :: Float64 = 1.0;   # 0.5x to 2.0x time stretch
attack_ms    :: Float64 = 5.0;   # Envelope attack
release_ms   :: Float64 = 150.0; # Envelope release
slice_mode   :: Int64   = 1;     # 0 = MANUAL, 1 = EQUAL CHOP (8 PADS), 2 = TRANSIENT
active_pad   :: Int64   = 0;     # 0 to 7 active triggering pad
active_knob  :: Int64   = 0;     # Dragged parameter knob

track_duration :: Float64 = 180.0; # Total track length in seconds (simulated)
playhead_pos   :: Float64 = 0.15;  # Normalized playhead position [0.0 .. 1.0]

# --- 8-PAD SLICE BANK ARRAYS ---
pad_start :: Array = [];
pad_end   :: Array = [];
pad_muted :: Array = [];
pad_solo  :: Array = [];

through p :: 0..7 -> loop {
    start_p :: Float64 = p * 0.125;
    end_p   :: Float64 = (p + 1) * 0.125;
    pad_start.push(start_p);
    pad_end.push(end_p);
    pad_muted.push(0);
    pad_solo.push(0);
};

# --- DYNAMIC WAVEFORM PEAK BUFFER ---
wave_peaks :: Array = [];
through i :: 0..299 -> loop {
    val = (vmath.sin(i * 0.08) * 0.4) + (vmath.cos(i * 0.23) * 0.35) + (vmath.sin(i * 0.51) * 0.25);
    wave_peaks.push(vmath.abs(val));
};

# --- UI HELPER: ROTARY CONTROL KNOB ---
fn draw_sampler_knob(name, x, y, val_norm, display_val, color, is_hovered, is_active) {
    radius_base = 36.0;
    if (is_hovered) { radius_base = 39.0; }
    if (is_active)  { radius_base = 41.0; }

    vglib.circle(x, y, radius_base, vglib.BLACK);
    
    if (is_active) {
        vglib.circle(x, y, radius_base - 2.0, color);
    } else {
        vglib.circle(x, y, radius_base - 3.0, vglib.rgba(45, 48, 58, 255));
    }

    vglib.circle(x, y, 28.0, vglib.rgba(20, 22, 28, 255));
    vglib.circle(x, y, 18.0, vglib.rgba(30, 34, 42, 255));
    
    angle = (val_norm * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x :: Float64 = x + vmath.sin(rad) * 25.0;
    line_y :: Float64 = y - vmath.cos(rad) * 25.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 3.5, color);
    
    label_col = is_hovered ? vglib.rgba(0, 230, 255, 255) : vglib.WHITE;
    vglib.text_ex(vcr_font, name, x - 22, y + 42, 11, label_col);
    vglib.text_ex(vcr_font, display_val, x - 18, y - 4, 10, color);
}

# --- UI HELPER: MPC PAD BUTTON ---
fn draw_mpc_pad(id, x, y, w, h, pad_color, is_active, is_hovered, start_t, end_t) {
    bg_col   = is_active ? pad_color : vglib.rgba(24, 28, 36, 255);
    rim_col  = is_active ? vglib.WHITE : (is_hovered ? pad_color : vglib.rgba(50, 58, 72, 255));
    text_col = is_active ? vglib.BLACK : vglib.WHITE;

    vglib.rect(x, y, w, h, bg_col);
    vglib.line(x, y, x + w, y, rim_col);
    vglib.line(x + w, y, x + w, y + h, rim_col);
    vglib.line(x + w, y + h, x, y + h, rim_col);
    vglib.line(x, y + h, x, y, rim_col);

    pad_str = "PAD " + string(id + 1);
    vglib.text_ex(vcr_font, pad_str, x + 12, y + 12, 13, text_col);

    start_sec = vmath.round(start_t * track_duration * 10.0) / 10.0;
    end_sec   = vmath.round(end_t * track_duration * 10.0) / 10.0;
    range_str = string(start_sec) + "s - " + string(end_sec) + "s";
    
    sub_col = is_active ? vglib.BLACK : vglib.rgba(140, 150, 165, 255);
    vglib.text_ex(vcr_font, range_str, x + 12, y + 36, 10, sub_col);

    if (is_active) {
        vglib.rect(x + w - 45, y + 10, 35, 16, vglib.BLACK);
        vglib.text_ex(vcr_font, "PLAY", x + w - 40, y + 13, 9, vglib.rgba(50, 255, 120, 255));
    }
}

# --- MAIN SAMPLER ENGINE RENDER LOOP ---
while (vglib.running()) {
    run_time = run_time + 0.016;

    # 2. UPDATE STREAM EVERY FRAME
    vaudio.update_stream(track);

    m = vglib.mouse_pos();
    md = vglib.mouse_delta();
    mouse_click = vglib.mouse_down(vglib.MOUSE_LEFT);

    # Update Simulated Playhead Movement
    if (vaudio.is_playing(track) == 1) {
        playhead_pos = playhead_pos + (0.0003 * speed_rate);
        if (playhead_pos > pad_end[active_pad]) {
            playhead_pos = pad_start[active_pad];
        }
    }

    # --- MOUSE CLICK DETECTIONS ---
    if (mouse_click && prev_mouse_state == 0) {
        # 1. Slice Mode Switching Bar
        if (m[1] >= 20 && m[1] <= 52) {
            if (m[0] >= 800 && m[0] <= 920) { slice_mode = 0; }
            if (m[0] >= 930 && m[0] <= 1050) {
                slice_mode = 1;
                through p :: 0..7 -> loop {
                    pad_start[p] = p * 0.125;
                    pad_end[p]   = (p + 1) * 0.125;
                };
            }
            if (m[0] >= 1060 && m[0] <= 1180) { slice_mode = 2; }
        }

        # 2. Clicking Main Overview Waveform
        if (m[0] >= 50 && m[0] <= 1350 && m[1] >= 80 && m[1] <= 240) {
            norm_click = (m[0] - 50.0) / 1300.0;
            playhead_pos = norm_click;
            
            through p :: 0..7 -> loop {
                if (norm_click >= pad_start[p] && norm_click <= pad_end[p]) {
                    active_pad = p;
                }
            };

            target_sec :: Float64 = playhead_pos * track_duration;
            vaudio.seek_stream(track, target_sec);
        }

        # 3. MPC Pad Grid Interaction (8 Pads)
        pad_w :: Int64 = 300;
        pad_h :: Int64 = 85;
        through p :: 0..7 -> loop {
            row :: Int64 = p / 4;
            col :: Int64 = p % 4;
            px :: Int64 = 50 + (col * 325);
            py :: Int64 = 580 + (row * 105);

            if (m[0] >= px && m[0] <= px + pad_w && m[1] >= py && m[1] <= py + pad_h) {
                active_pad = p;
                playhead_pos = pad_start[p];
                
                start_seconds :: Float64 = pad_start[p] * track_duration;
                
                # 3. SEEK STREAM DIRECTLY TO TARGET TIMESTAMP
                vaudio.seek_stream(track, start_seconds);
            }
        };
    }

    # --- KNOB DRAGGING INTERACTION ---
    hk1 = vmath.hypot(m[0] - 120, m[1] - 470) < 38;
    hk2 = vmath.hypot(m[0] - 260, m[1] - 470) < 38;
    hk3 = vmath.hypot(m[0] - 400, m[1] - 470) < 38;
    hk4 = vmath.hypot(m[0] - 540, m[1] - 470) < 38;

    if (mouse_click) {
        if (active_knob == 0) {
            if (hk1) { active_knob = 1; }
            if (hk2) { active_knob = 2; }
            if (hk3) { active_knob = 3; }
            if (hk4) { active_knob = 4; }
        }

        delta = md[1] * 0.3;
        if (active_knob == 1) { pitch_shift = vmath.clamp(pitch_shift - (delta * 0.2), -12.0, 12.0); }
        if (active_knob == 2) { speed_rate  = vmath.clamp(speed_rate - (delta * 0.01), 0.5, 2.0); }
        if (active_knob == 3) { attack_ms   = vmath.clamp(attack_ms - delta, 0.1, 100.0); }
        if (active_knob == 4) { release_ms  = vmath.clamp(release_ms - (delta * 2.0), 10.0, 1000.0); }
    } else {
        active_knob = 0;
    }

    prev_mouse_state = mouse_click ? 1 : 0;

    vaudio.set_pitch(track, vmath.pow(2.0, pitch_shift / 12.0));

    vglib.begin();
        vglib.clear(vglib.rgba(14, 16, 22, 255));

        # HEADER BAR
        vglib.text_ex(vcr_font, "VYNE SERATO SAMPLER", 50, 26, 18, vglib.WHITE);
        vglib.text_ex(vcr_font, "KEY: Fm | BPM: 130.0", 300, 28, 12, vglib.rgba(0, 220, 255, 255));

        mode0_col = (slice_mode == 0) ? vglib.rgba(0, 220, 255, 255) : vglib.rgba(30, 36, 48, 255);
        mode1_col = (slice_mode == 1) ? vglib.rgba(0, 220, 255, 255) : vglib.rgba(30, 36, 48, 255);
        mode2_col = (slice_mode == 2) ? vglib.rgba(0, 220, 255, 255) : vglib.rgba(30, 36, 48, 255);

        vglib.rect(800, 20, 120, 32, mode0_col);
        vglib.text_ex(vcr_font, "MANUAL", 830, 30, 11, (slice_mode == 0) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(930, 20, 120, 32, mode1_col);
        vglib.text_ex(vcr_font, "CHOP (8)", 952, 30, 11, (slice_mode == 1) ? vglib.BLACK : vglib.WHITE);

        vglib.rect(1060, 20, 120, 32, mode2_col);
        vglib.text_ex(vcr_font, "TRANSIENT", 1076, 30, 11, (slice_mode == 2) ? vglib.BLACK : vglib.WHITE);

        vglib.line(50, 62, 1350, 62, vglib.rgba(45, 52, 66, 255));

        # MODULE 1: FULL TRACK OVERVIEW WAVEFORM
        vglib.rect(50, 80, 1300, 160, vglib.rgba(18, 20, 28, 255));
        vglib.line(50, 80, 1350, 80, vglib.rgba(50, 58, 72, 255));
        vglib.line(1350, 80, 1350, 240, vglib.rgba(50, 58, 72, 255));
        vglib.line(1350, 240, 50, 240, vglib.rgba(50, 58, 72, 255));
        vglib.line(50, 240, 50, 80, vglib.rgba(50, 58, 72, 255));

        pad_colors :: Array = [
            vglib.rgba(255, 70, 70, 255),
            vglib.rgba(255, 160, 40, 255),
            vglib.rgba(255, 220, 50, 255),
            vglib.rgba(50, 230, 120, 255),
            vglib.rgba(0, 220, 255, 255),
            vglib.rgba(160, 90, 255, 255),
            vglib.rgba(255, 100, 200, 255),
            vglib.rgba(180, 210, 245, 255)
        ];

        through p :: 0..7 -> loop {
            p_st :: Float64 = pad_start[p];
            p_en :: Float64 = pad_end[p];

            rx :: Float64 = 50.0 + (p_st * 1300.0);
            rw :: Float64 = (p_en - p_st) * 1300.0;
            p_col = pad_colors[p];

            if (p == active_pad) {
                vglib.rect(rx, 81, rw, 158, vglib.rgba(255, 255, 255, 25));
            }

            vglib.line(rx, 80, rx, 240, p_col);
            vglib.rect(rx, 80, 24, 16, p_col);
            vglib.text_ex(vcr_font, string(p + 1), rx + 8, 83, 11, vglib.BLACK);
        };

        num_peaks :: Float64 = wave_peaks.length();
        step_w    :: Float64 = 1300.0 / num_peaks;
        cy        :: Float64 = 160.0;

        through idx :: 0..(int64(num_peaks) - 1) -> loop {
            pk = wave_peaks[idx];
            px :: Float64 = 50.0 + (idx * step_w);
            ph :: Float64 = pk * 65.0;

            norm_x = idx / num_peaks;
            peak_col = vglib.rgba(120, 135, 160, 180);
            through p :: 0..7 -> loop {
                if (norm_x >= pad_start[p] && norm_x <= pad_end[p]) {
                    peak_col = pad_colors[p];
                }
            };

            vglib.rect(px, cy - ph, step_w - 0.5, ph * 2.0, peak_col);
        };

        ph_x :: Float64 = 50.0 + (playhead_pos * 1300.0);
        vglib.line(ph_x, 80, ph_x, 240, vglib.WHITE);
        vglib.circle(ph_x, 80, 5.0, vglib.WHITE);

        # MODULE 2: ZOOMED ACTIVE SLICE WAVEFORM
        vglib.rect(50, 260, 1300, 140, vglib.rgba(10, 12, 16, 255));
        vglib.line(50, 260, 1350, 260, vglib.rgba(45, 52, 66, 255));
        vglib.line(1350, 260, 1350, 400, vglib.rgba(45, 52, 66, 255));
        vglib.line(1350, 400, 50, 400, vglib.rgba(45, 52, 66, 255));
        vglib.line(50, 400, 50, 260, vglib.rgba(45, 52, 66, 255));

        act_col = pad_colors[active_pad];
        vglib.text_ex(vcr_font, "ZOOMED SLICE VIEW - PAD " + string(active_pad + 1), 65, 272, 11, act_col);

        prev_zx :: Float64 = 50.0;
        prev_zy :: Float64 = 330.0;
        z_step  :: Float64 = 10.0;
        curr_zx :: Float64 = 50.0;

        while (curr_zx <= 1350.0) {
            norm_z = (curr_zx - 50.0) / 1300.0;
            z_amp  = vmath.sin(norm_z * 40.0 + run_time * 5.0) * vmath.cos(norm_z * 12.0) * 50.0;
            curr_zy :: Float64 = 330.0 - z_amp;

            if (curr_zx > 50.0) {
                vglib.line(prev_zx, prev_zy, curr_zx, curr_zy, act_col);
            }
            prev_zx = curr_zx;
            prev_zy = curr_zy;
            curr_zx = curr_zx + z_step;
        }

        vglib.line(50, 330, 1350, 330, vglib.rgba(35, 42, 54, 255));

        # MODULE 3: CONTROLS
        p_norm   = (pitch_shift + 12.0) / 24.0;
        s_norm   = (speed_rate - 0.5) / 1.5;
        a_norm   = (attack_ms - 0.1) / 99.9;
        r_norm   = (release_ms - 10.0) / 990.0;

        p_str = (pitch_shift >= 0.0 ? "+" : "") + string(vmath.round(pitch_shift * 10.0) / 10.0) + "st";
        s_str = string(vmath.round(speed_rate * 100.0) / 100.0) + "x";
        a_str = string(vmath.round(attack_ms)) + "ms";
        r_str = string(vmath.round(release_ms)) + "ms";

        draw_sampler_knob("PITCH", 120, 470, p_norm, p_str, vglib.rgba(0, 220, 255, 255), hk1, active_knob == 1);
        draw_sampler_knob("SPEED", 260, 470, s_norm, s_str, vglib.rgba(255, 160, 40, 255), hk2, active_knob == 2);
        draw_sampler_knob("ATTACK", 400, 470, a_norm, a_str, vglib.rgba(50, 230, 120, 255), hk3, active_knob == 3);
        draw_sampler_knob("RELEASE", 540, 470, r_norm, r_str, vglib.rgba(160, 90, 255, 255), hk4, active_knob == 4);

        vglib.rect(680, 425, 670, 95, vglib.rgba(18, 22, 30, 255));
        vglib.line(680, 425, 1350, 425, vglib.rgba(45, 52, 66, 255));
        vglib.line(1350, 425, 1350, 520, vglib.rgba(45, 52, 66, 255));
        vglib.line(1350, 520, 680, 520, vglib.rgba(45, 52, 66, 255));
        vglib.line(680, 520, 680, 425, vglib.rgba(45, 52, 66, 255));

        vglib.text_ex(vcr_font, "ACTIVE CUE INFO", 700, 440, 11, vglib.rgba(160, 170, 185, 255));
        st_val = vmath.round(pad_start[active_pad] * track_duration * 100.0) / 100.0;
        en_val = vmath.round(pad_end[active_pad] * track_duration * 100.0) / 100.0;
        
        info_str = "PAD " + string(active_pad + 1) + " | START: " + string(st_val) + "s | END: " + string(en_val) + "s";
        vglib.text_ex(vcr_font, info_str, 700, 462, 13, act_col);
        vglib.text_ex(vcr_font, "ALGORITHM: VYNE TIME-STRETCH v1.0 (FORMANT PRESERVED)", 700, 490, 10, vglib.rgba(100, 110, 130, 255));

        # MODULE 4: MPC PAD GRID
        pad_w :: Int64 = 300;
        pad_h :: Int64 = 85;

        through p :: 0..7 -> loop {
            row :: Int64 = p / 4;
            col :: Int64 = p % 4;

            px :: Int64 = 50 + (col * 325);
            py :: Int64 = 580 + (row * 105);

            is_act = (p == active_pad) && (vaudio.is_playing(track) == 1);
            is_hov = (m[0] >= px && m[0] <= px + pad_w && m[1] >= py && m[1] <= py + pad_h);
            
            draw_mpc_pad(p, px, py, pad_w, pad_h, pad_colors[p], is_act, is_hov, pad_start[p], pad_end[p]);
        };

        vglib.text_ex(vcr_font, "VYNE AUDIO ENGINE v1.0.0 | SERATO SAMPLE DSP ENGINE", 460, 860, 12, vglib.rgba(120, 130, 150, 255));

    vglib.end();
}

vaudio.close_audio();
vglib.close();