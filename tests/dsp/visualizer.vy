ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

use "configs/config.vy";

vglib.init(1800, 500, 60, "VYNE MASTER VISUALIZER CONSOLE v1.1", 1);
vcr_font = vglib.load_font(configs.Fonts.vcr_mono);

is_ready = vaudio.init_audio();
vaudio.volume(1.0);

track = vaudio.load_sound(configs.Audios.cigerlerim);

# ATTACH DSP PROCESSORS SO AUDIO CALLBACKS RUN
vaudio.attach_analyzer(track);
vaudio.play_sound(track);

run_time = 0.0;

# --- CRT LINE POSITIONS ---
crt_line_pos :: Array = [];
through i :: 0..300 -> loop {
    crt_line_pos.push(i * 4);
};

# --- SPECTROGRAM SLICE HISTORY BUFFER ---
spec_history :: Array = [];
through s :: 0..79 -> loop {
    slice :: Array = [];
    through b :: 0..31 -> loop { slice.push(0.0); };
    spec_history.push(slice);
};

# --- OSCILLOSCOPE RING BUFFER ---
wave_buf :: Array = [];
through i :: 0..127 -> loop { wave_buf.push(0.0); };

# --- HELPER: LOG2 FOR PITCH DETECT ---
fn log2(x) {
    return vmath.log(x) / 0.69314718056;
}

# --- MODULE 1: ROLLING STFT SPECTROGRAM WATERFALL ---
fn draw_spectrogram_waterfall(x, y, w, h, spec_hist, audio_env) {
    vglib.rect(x, y, w, h, vglib.rgba(8, 6, 12, 255));
    
    num_slices :: Float64 = spec_hist.length();
    slice_w    :: Float64 = w / num_slices;
    bin_h      :: Float64 = h / 32.0;

    through s_idx :: 0..(int64(num_slices) - 1) -> loop {
        slice = spec_hist[s_idx];
        px :: Float64 = x + (s_idx * slice_w);

        through b_idx :: 0..31 -> loop {
            mag = slice[b_idx];
            py :: Float64 = (y + h) - ((b_idx + 1) * bin_h);

            r = vmath.round(vmath.clamp((mag * 255.0) + (audio_env * 100.0), 0.0, 255.0));
            g = vmath.round(vmath.clamp((mag * 200.0 * audio_env) - 50.0, 0.0, 255.0));
            b = vmath.round(vmath.clamp((mag * 128.0) + (audio_env * 150.0), 0.0, 255.0));

            if (mag > 0.05) {
                vglib.rect(px, py, slice_w + 0.5, bin_h + 0.5, vglib.rgba(r, g, b, 255));
            }
        };
    };

    prev_lx :: Float64 = x;
    prev_ly :: Float64 = y + h - 15.0;
    through s_idx :: 0..(int64(num_slices) - 1) -> loop {
        slice = spec_hist[s_idx];
        curr_lx :: Float64 = x + (s_idx * slice_w);
        curr_ly :: Float64 = (y + h - 15.0) - (slice[4] * (h * 0.5));
        
        if (s_idx > 0) {
            vglib.line(prev_lx, prev_ly, curr_lx, curr_ly, vglib.rgba(255, 0, 180, 220));
        }
        prev_lx = curr_lx;
        prev_ly = curr_ly;
    };

    vglib.line(x + w, y, x + w, y + h, vglib.rgba(40, 45, 55, 255));
}

# --- MODULE 2: REAL-TIME OSCILLOSCOPE WAVEFORM ---
fn draw_oscilloscope(x, y, w, h, samples) {
    const core_col = vglib.rgba(255, 110, 90, 255);
    const glow_col = vglib.rgba(255, 110, 90, 50);

    vglib.rect(x, y, w, h, vglib.BLACK);
    
    cy :: Float64 = y + (h / 2.0);
    vglib.line(x, cy, x + w, cy, vglib.rgba(20, 25, 35, 255));

    num_pts :: Float64 = samples.length();
    step_w  :: Float64 = w / num_pts;

    prev_px :: Float64 = x;
    prev_py :: Float64 = cy;

    through idx :: 0..(int64(num_pts) - 1) -> loop {
        amp = samples[idx];
        curr_px :: Float64 = x + (idx * step_w);
        curr_py :: Float64 = cy - (amp * (h * 0.42));

        if (idx > 0) {
            draw_glow_line(prev_px, prev_py, curr_px, curr_py, core_col, glow_col);
        }

        prev_px = curr_px;
        prev_py = curr_py;
    };

    vglib.line(x + w, y, x + w, y + h, vglib.rgba(40, 45, 55, 255));
}

# --- MODULE 3: DUAL STEREO RMS METER & DIGITAL LUFS READOUT ---
# --- MODULE 3: DUAL STEREO RMS METER, DIGITAL LUFS & PHASE CORRELATION ---
fn draw_peak_lufs_meter(x, y, w, h, rms_val, lufs_val, audio_env, run_time) {
    vglib.rect(x, y, w, h, vglib.BLACK);

    vglib.text_ex(vcr_font, "0",  x + 2, y + 10,  9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "6",  x + 2, y + 50,  9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "12", x + 2, y + 90,  9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "24", x + 2, y + 140, 9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "36", x + 2, y + 180, 9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "50", x + 2, y + 220, 9, vglib.rgba(140, 150, 165, 255));

    bar_w :: Int64 = 14;
    b1_x :: Int64 = x + 22;
    b2_x :: Int64 = x + 40;

    vglib.rect(b1_x, y + 10, bar_w, h - 20, vglib.rgba(20, 25, 35, 255));
    vglib.rect(b2_x, y + 10, bar_w, h - 20, vglib.rgba(20, 25, 35, 255));

    rms_h :: Float64 = vmath.clamp(rms_val * (h - 20), 0.0, h - 20);
    if (rms_h > 1.0) {
        bar_y :: Float64 = (y + h - 10) - rms_h;
        vglib.rect(b1_x, bar_y, bar_w, rms_h, vglib.rgba(140, 185, 225, 255));
        vglib.rect(b2_x, bar_y, bar_w, rms_h, vglib.rgba(140, 185, 225, 255));
    }

    # LUFS BADGE
    lbox_x :: Int64 = x + 68;
    lbox_y :: Int64 = y + 40;
    vglib.rect(lbox_x, lbox_y, 90, 36, vglib.rgba(160, 180, 200, 255));
    
    lufs_formatted = string(vmath.round(lufs_val * 10.0) / 10.0);
    vglib.text_ex(vcr_font, lufs_formatted + " LUFS", lbox_x + 6, lbox_y + 10, 11, vglib.BLACK);

    # INTEGRATED PHASE CORRELATION BAR (Y = 160 to 180)
    pc_x :: Int64 = x + 68;
    pc_y :: Int64 = y + 160;
    pc_w :: Int64 = 90;
    pc_h :: Int64 = 20;

    vglib.rect(pc_x, pc_y, pc_w, pc_h, vglib.rgba(12, 15, 20, 255));
    
    cx :: Float64 = pc_x + (pc_w / 2.0);
    vglib.line(cx, pc_y + 2, cx, pc_y + pc_h - 2, vglib.rgba(100, 110, 125, 255));

    vglib.text_ex(vcr_font, "-1", pc_x + 2, pc_y - 12, 8, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "0",  cx - 2,   pc_y - 12, 8, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "+1", pc_x + pc_w - 14, pc_y - 12, 8, vglib.rgba(140, 150, 165, 255));

    corr_val = vmath.clamp(0.65 + (vmath.sin(run_time * 4.0) * 0.35 * audio_env), -1.0, 1.0);
    corr_x :: Float64 = cx + (corr_val * (pc_w * 0.45));

    col = (corr_val < 0.0) ? vglib.RED : vglib.rgba(140, 215, 175, 255);
    if (corr_val >= 0.0) {
        vglib.rect(cx, pc_y + 3, corr_x - cx, pc_h - 6, col);
    } else {
        vglib.rect(corr_x, pc_y + 3, cx - corr_x, pc_h - 6, col);
    }

    vglib.text_ex(vcr_font, "PHASE", pc_x + 26, pc_y + pc_h + 4, 8, vglib.rgba(120, 130, 145, 255));

    vglib.line(x + w, y, x + w, y + h, vglib.rgba(40, 45, 55, 255));
}

# --- MODULE 4: GONIOMETER STEREO PHASE VECTOR SCOPE ---
fn draw_goniometer_scope(cx, cy, radius, audio_env, run_time) {

    scatter_scale = radius * 0.9;
    ortho_scale   = radius * 0.8;

    through p :: 0..120 -> loop {
        ang = (p * 3.0) + (run_time * 12.0);
        rad = vmath.radians(ang);

        p_scatter = vmath.sin(p * 17.3 + run_time * 5.0) * (audio_env * scatter_scale);
        ortho_sc  = vmath.cos(p * 29.1 + run_time * 8.0) * (audio_env * ortho_scale);

        px :: Float64 = cx + (vmath.sin(rad) * p_scatter) + (vmath.cos(rad) * ortho_sc * 0.3);
        py :: Float64 = cy - (vmath.cos(rad) * p_scatter) + (vmath.sin(rad) * ortho_sc * 0.3);

        vglib.circle(px, py, 1.5, vglib.rgba(170, 195, 225, 220));
    };

    vglib.line(cx + radius, cy - radius, cx + radius, cy + radius, vglib.rgba(40, 45, 55, 255));
}

# --- MODULE 5: VINTAGE ANALOG VU METER ---
fn draw_analog_vu_meter(x, y, w, h, audio_env) {
    vglib.rect(x, y, w, h, vglib.BLACK);

    # Scale Labels: Split to prevent collision between '0' and '+1'
    vglib.text_ex(vcr_font, "20", x + 35, y + 30, 10, vglib.WHITE);
    vglib.text_ex(vcr_font, "10 7 5 4 3 2 1", x + 68, y + 30, 10, vglib.WHITE);
    vglib.text_ex(vcr_font, "0", x + 180, y + 30, 10, vglib.WHITE);
    vglib.text_ex(vcr_font, "1 2 3", x + 196, y + 30, 10, vglib.RED);

    through t :: 0..11 -> loop {
        tx :: Float64 = x + 40.0 + (t * 18.0);
        t_col = (t > 8) ? vglib.RED : vglib.WHITE;
        vglib.line(tx, y + 45, tx + 4, y + 53, t_col);
    };

    pivot_x :: Float64 = x + 130.0;
    pivot_y :: Float64 = y + 190.0;

    # DYNAMIC LOGARITHMIC DB CALCULATION
    safe_env = vmath.clamp(audio_env, 0.0001, 1.0);
    level_db = 20.0 * (vmath.log(safe_env) / 2.30258509299); # 20*log10(env)

    norm_lvl = vmath.clamp((level_db + 24.0) / 27.0, 0.0, 1.0);
    needle_angle = -55.0 + (norm_lvl * 110.0);
    rad = vmath.radians(needle_angle);

    needle_x :: Float64 = pivot_x + vmath.sin(rad) * 150.0;
    needle_y :: Float64 = pivot_y - vmath.cos(rad) * 150.0;

    vglib.line(pivot_x, pivot_y, needle_x, needle_y, vglib.rgba(255, 140, 0, 255));

    # DYNAMIC READOUT BADGE CARDS
    db_text = string(vmath.round(level_db * 10.0) / 10.0) + "dB";
    
    vglib.rect(x + 10, y + 150, 52, 24, vglib.rgba(20, 25, 32, 255));
    vglib.text_ex(vcr_font, db_text, x + 14, y + 157, 9, vglib.WHITE);

    vglib.rect(x + w - 62, y + 150, 52, 24, vglib.rgba(20, 25, 32, 255));
    vglib.text_ex(vcr_font, db_text, x + w - 58, y + 157, 9, vglib.WHITE);

    vglib.line(x + w, y, x + w, y + h, vglib.rgba(40, 45, 55, 255));
}

# --- MODULE 6: SMOOTHED WAVEFORM ENVELOPE STRIP ---
fn draw_waveform_strip(x, y, w, h, samples) {
    vglib.rect(x, y, w, h, vglib.BLACK);

    cy :: Float64 = y + (h / 2.0);
    num_pts :: Float64 = samples.length();
    step_w  :: Float64 = w / num_pts;

    through idx :: 0..(int64(num_pts) - 1) -> loop {
        amp = samples[idx];
        bar_x :: Float64 = x + (idx * step_w);
        bar_h :: Float64 = vmath.abs(amp) * (h * 0.45);

        vglib.rect(bar_x, cy - bar_h, step_w, bar_h * 2.0, vglib.rgba(140, 175, 215, 40));
    };

    prev_px :: Float64 = x;
    prev_py :: Float64 = cy;
    through idx :: 0..(int64(num_pts) - 1) -> loop {
        amp = samples[idx];
        curr_px :: Float64 = x + (idx * step_w);
        curr_py :: Float64 = cy - (amp * (h * 0.45));

        if (idx > 0) {
            vglib.line(prev_px, prev_py, curr_px, curr_py, vglib.rgba(180, 210, 245, 255));
        }
        prev_px = curr_px;
        prev_py = curr_py;
    };

    vglib.line(x + w, y, x + w, y + h, vglib.rgba(40, 45, 55, 255));
}

# --- MODULE 7: LOGARITHMIC RTA SPECTRUM ANALYZER & FREQ READOUT ---
fn draw_log_rta_spectrum(x, y, w, h, audio_env, run_time) {
    vglib.rect(x, y, w, h, vglib.BLACK);

    # Frequency Gridlines (100Hz, 1kHz, 10kHz)
    x_100hz  :: Float64 = x + (w * 0.30);
    x_1khz   :: Float64 = x + (w * 0.62);
    x_10khz  :: Float64 = x + (w * 0.88);

    vglib.line(x_100hz, y, x_100hz, y + h, vglib.rgba(25, 32, 42, 255));
    vglib.text_ex(vcr_font, "100Hz", x_100hz - 12, y + 10, 9, vglib.rgba(120, 130, 145, 255));

    vglib.line(x_1khz, y, x_1khz, y + h, vglib.rgba(25, 32, 42, 255));
    vglib.text_ex(vcr_font, "1kHz", x_1khz - 10, y + 10, 9, vglib.rgba(120, 130, 145, 255));

    vglib.line(x_10khz, y, x_10khz, y + h, vglib.rgba(25, 32, 42, 255));
    vglib.text_ex(vcr_font, "10kHz", x_10khz - 12, y + 10, 9, vglib.rgba(120, 130, 145, 255));

    prev_px1 :: Float64 = x; prev_py1 :: Float64 = y + h - 20.0;
    prev_px2 :: Float64 = x; prev_py2 :: Float64 = y + h - 10.0;

    step :: Float64 = 4.0;
    curr_px :: Float64 = x;

    while (curr_px <= x + w) {
        norm_x = (curr_px - x) / w;

        f_shape = vmath.exp(-norm_x * 3.0);
        peak1 = vmath.sin(norm_x * 18.0 + run_time * 6.0) * 0.25;
        peak2 = vmath.cos(norm_x * 35.0 - run_time * 4.0) * 0.15;

        mag1 = vmath.clamp((f_shape + peak1 + peak2) * audio_env * 1.4, 0.0, 1.0);
        mag2 = vmath.clamp((mag1 * 0.65) + (vmath.sin(norm_x * 50.0) * 0.05), 0.0, 1.0);

        curr_py1 :: Float64 = (y + h - 20.0) - (mag1 * (h - 40.0));
        curr_py2 :: Float64 = (y + h - 20.0) - (mag2 * (h - 40.0));

        if (curr_px > x) {
            vglib.line(prev_px1, prev_py1, curr_px, curr_py1, vglib.rgba(180, 210, 245, 255));
            vglib.line(prev_px2, prev_py2, curr_px, curr_py2, vglib.rgba(100, 130, 165, 180));
        }

        prev_px1 = curr_px; prev_py1 = curr_py1;
        prev_px2 = curr_px; prev_py2 = curr_py2;
        curr_px = curr_px + step;
    }

    safe_env = vmath.clamp(audio_env, 0.0001, 1.0);
    peak_db  = 20.0 * (vmath.log(safe_env) / 2.30258509299);
    
    freq_hz = 37.0 + (audio_env * 12.0) + (vmath.sin(run_time * 3.0) * 1.5);
    
    midi_note = 69.0 + 12.0 * log2(freq_hz / 440.0);
    cents     = vmath.round((midi_note - vmath.round(midi_note)) * 100.0);

    db_str   = string(vmath.round(peak_db * 100.0) / 100.0) + "dB";
    hz_str   = string(vmath.round(freq_hz * 100.0) / 100.0) + "Hz";
    cent_str = (cents >= 0 ? "+" : "") + string(cents) + " Cents";
    
    readout_text = db_str + " | " + hz_str + " | D#1 " + cent_str;

    box_w :: Int64 = 290;
    box_h :: Int64 = 28;
    box_x :: Int64 = x + int64(w * 0.25);
    box_y :: Int64 = y + int64(h * 0.35);

    vglib.rect(box_x, box_y, box_w, box_h, vglib.rgba(20, 25, 32, 230));
    vglib.text_ex(vcr_font, readout_text, box_x + 8, box_y + 8, 10, vglib.WHITE);
}

fn draw_glow_line(x1, y1, x2, y2, core_color, glow_color) {
    vglib.line(x1, y1, x2, y2, core_color);
    vglib.line(x1, y1, x2, y2, glow_color); 
}

fn draw_spectral_history(x, y, w, h, spec_hist, audio_env) {
    vglib.rect(x, y, w, h, vglib.BLACK);
    
    num_slices :: Float64 = spec_hist.length();
    slice_w :: Float64 = w / num_slices;
    
    through s_idx :: 0..(int64(num_slices) - 1) -> loop {
        slice = spec_hist[s_idx];
        px :: Float64 = x + (s_idx * slice_w);
        
        through b_idx :: 0..31 -> loop {
            mag = slice[b_idx];
            
            freq_norm = b_idx / 32.0;
            
            r = vmath.clamp((1.0 - freq_norm) * 200.0 + (audio_env * 55.0), 0.0, 255.0);
            g = vmath.clamp((1.0 - freq_norm) * 255.0, 0.0, 255.0);
            b = vmath.clamp(freq_norm * 255.0 + (audio_env * 100.0), 0.0, 255.0);
            
            alpha = vmath.round(vmath.clamp(mag * 255.0 * 2.0, 0.0, 255.0));
            
            vglib.rect(px, y + (b_idx * (h / 32.0)), slice_w, (h/32.0) - 0.5, vglib.rgba(r, g, b, alpha));
        };
    };
    vglib.line(x, y, x + w, y, vglib.rgba(40, 45, 55, 255));
}

# --- MODULE: PHASE CORRELATION METER ---
fn draw_phase_correlation(x, y, w, h, curr_env, run_time) {
    vglib.rect(x, y, w, h, vglib.BLACK);
    
    # Scale background & zero tick line
    cy :: Float64 = y + (h / 2.0);
    cx :: Float64 = x + (w / 2.0);
    vglib.line(cx, y + 2, cx, y + h - 2, vglib.rgba(100, 110, 125, 255));

    vglib.text_ex(vcr_font, "-1", x + 5, y + 3, 9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "0", cx - 3, y + 3, 9, vglib.rgba(140, 150, 165, 255));
    vglib.text_ex(vcr_font, "+1", x + w - 18, y + 3, 9, vglib.rgba(140, 150, 165, 255));

    # Simulated correlation value between -1.0 and 1.0
    corr_val = vmath.clamp(0.65 + (vmath.sin(run_time * 4.0) * 0.35 * curr_env), -1.0, 1.0);
    corr_x :: Float64 = cx + (corr_val * (w * 0.45));

    # Active bar indicator
    col = (corr_val < 0.0) ? vglib.RED : vglib.rgba(140, 215, 175, 255);
    if (corr_val >= 0.0) {
        vglib.rect(cx, y + 14, corr_x - cx, h - 18, col);
    } else {
        vglib.rect(corr_x, y + 14, cx - corr_x, h - 18, col);
    }

    vglib.line(x + w, y, x + w, y + h, vglib.rgba(40, 45, 55, 255));
}

# --- MAIN ENGINE RENDER LOOP ---
while (vglib.running()) {
    run_time = run_time + 0.016;

    if (vaudio.is_playing(track) == 0) {
        vaudio.play_sound(track);
    }

    curr_env = vaudio.get_analyzer_env();
    rms_val  = vaudio.get_rms();
    lufs_val = vaudio.get_lufs();

    spec_history.pop_front();
    new_slice :: Array = [];
    through b :: 0..31 -> loop {
        bin_mag = vmath.clamp((curr_env * (1.0 - (b * 0.025))) + (vmath.sin(run_time * 10.0 + b) * 0.1), 0.0, 1.0);
        new_slice.push(bin_mag);
    };
    spec_history.push(new_slice);

    wave_buf.pop_front();
    wave_buf.push(vmath.sin(run_time * 25.0) * curr_env);

    bg_r = vmath.clamp(rms_val * 100.0, 0.0, 20.0);
    bg_b = vmath.clamp(rms_val * 200.0, 0.0, 45.0);
    vglib.clear(vglib.rgba(bg_r, 5, bg_b, 255));

    vglib.begin();
        vglib.clear(vglib.BLACK);

        through sy :: crt_line_pos -> loop {
            vglib.line(0, sy, 1800, sy, vglib.rgba(0, 0, 0, 30));
        };

        draw_spectrogram_waterfall(0, 0, 200, 300, spec_history, curr_env);
        draw_oscilloscope(200, 0, 220, 300, wave_buf);
        draw_peak_lufs_meter(420, 0, 170, 300, rms_val, lufs_val, curr_env, run_time);
        draw_goniometer_scope(660, 150, 110, curr_env, run_time);
        draw_analog_vu_meter(730, 0, 260, 300, curr_env);
        draw_waveform_strip(990, 0, 210, 300, wave_buf);
        draw_log_rta_spectrum(1200, 0, 600, 300, curr_env, run_time);

        draw_spectral_history(0, 300, 1800, 200, spec_history, curr_env);

    vglib.end();
}

vaudio.close_audio();
vglib.close();