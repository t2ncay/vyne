ruleset { dynamic_casting };
module vglib;
module vnet;
module vcore;
module vmath;
module vaudio;

# Initialize Vyne Graphics Subsystem (1280x800, 60FPS)
vglib.init(1280, 800, 60, "VYNE VDEC CRACKING SUBSYSTEM v1.0", 0);
vcr_font = vglib.load_font("tests/assets/VCR_OSD_MONO_1.001.ttf");

# Audio Engine Initialization & SFX Setup
is_audio_ready = vaudio.init_audio();
vaudio.volume(1.0);

stream_crt_noise = vaudio.play_stream("tests/assets/crt_noise.mp3");
click_sfx        = vaudio.load_sound("tests/assets/mouse_click.mp3");

vaudio.sound_volume(stream_crt_noise, 0.80);
vaudio.sound_volume(click_sfx, 0.90);

# Color Palette Definitions
COLOR_BLACK  :: Int64 = vglib.rgba(2, 2, 4, 255);
COLOR_PANEL  :: Int64 = vglib.rgba(10, 12, 16, 245);
COLOR_BORDER :: Int64 = vglib.rgba(40, 50, 60, 255);
COLOR_BLOOD  :: Int64 = vglib.rgba(220, 20, 40, 255);
COLOR_AMBER  :: Int64 = vglib.rgba(255, 150, 0, 255);
COLOR_TOXIC  :: Int64 = vglib.rgba(40, 240, 100, 255);
COLOR_CYAN   :: Int64 = vglib.rgba(0, 220, 240, 255);
COLOR_GHOST  :: Int64 = vglib.rgba(160, 170, 185, 255);

# ====================================================================
# MINIGAME & ENGINE STATE
# ====================================================================
run_time             :: Float64 = 0.0;
crt_heat             :: Float64 = 35.0;
glitch_trigger       :: Float64 = 0.0;
mouse_was_down       :: Int64   = 0;

# VDEC Crack Session State
vdec_active          :: Int64   = 0;
vdec_timer           :: Float64 = 0.0;
vdec_duration        :: Float64 = 0.0;
vdec_raw_payload     :: Int64   = 12;
vdec_target_offset   :: Int64   = 4;
vdec_result_key      :: Int64   = 0;
vdec_success_flash   :: Float64 = 0.0;

terminal_logs        :: Array   = [
    "[SYS_INIT]: VDEC RING-0 KERNEL HOOKS MOUNTED",
    "[SYS_INIT]: READY TO EXECUTE SECTOR DECRYPTION",
    "--------------------------------------------------",
    "PRESS [SPACE] OR CLICK THE BUTTON TO START VDEC DECODE SEQUENCE."
];

# Helper Functions
fn clean_str(raw :: String) -> String {
    out_str = raw;
    while (out_str.length() > 0) {
        c = out_str.substr(0, 1);
        if (c == " " || c == "\"" || c == "\t" || c == "\r" || c == "\n") {
            out_str = out_str.substr(1, out_str.length() - 1);
        } else { break; }
    }
    return out_str;
}

fn trigger_vdec_crack(payload :: Int64, offset :: Int64) {
    if (vdec_active == 1) { return null; }
    
    vdec_active        = 1;
    vdec_timer         = 0.0;
    vdec_duration      = vmath.random(5.0, 8.0); # Random 5-8 second lockdown
    vdec_raw_payload   = payload;
    vdec_target_offset = offset;
    vdec_result_key    = int64(vmath.random(1000, 9999)); # Decoded key result
    
    glitch_trigger = 1.0;
    terminal_logs.push("[VDEC]: CRACKING LOCKOUT ENGAGED. INTERACTION FROZEN...");
}

# ====================================================================
# MAIN APPLICATION LOOP
# ====================================================================
while (vglib.running()) {
    vaudio.update_stream(stream_crt_noise);
    
    run_time = run_time + 0.016;

    m_pos   = vglib.mouse_pos();
    mx :: Float64 = float64(m_pos[0]);
    my :: Float64 = float64(m_pos[1]);
    m_down  :: Int64 = vglib.mouse_down(vglib.MOUSE_LEFT);
    m_click :: Int64 = (m_down == 1 && mouse_was_down == 0) ? 1 : 0;
    mouse_was_down   = m_down;

    # Decrease glitch decay
    if (glitch_trigger > 0.0) { glitch_trigger = glitch_trigger - 0.016; }
    if (vdec_success_flash > 0.0) { vdec_success_flash = vdec_success_flash - 0.016; }

    # Handle Key Press Start
    if (vglib.key_pressed(vglib.SPACE) && vdec_active == 0) {
        trigger_vdec_crack(12, 4);
    }

    # ================================================================
    # VDEC LOCKDOWN & BRUTE-FORCE TICK ENGINE
    # ================================================================
    if (vdec_active == 1) {
        vdec_timer = vdec_timer + 0.016;
        
        # Increase CRT Heat and Neural Strain during active decode
        crt_heat = vmath.clamp(crt_heat + (0.016 * 4.5), 35.0, 100.0);
        glitch_trigger = vmath.clamp(glitch_trigger + 0.02, 0.0, 0.8);

        # Decryption Completed
        if (vdec_timer >= vdec_duration) {
            vdec_active        = 0;
            vdec_success_flash = 2.0;
            glitch_trigger     = 1.2;
            
            terminal_logs.push("[VDEC SUCCESS]: KEY CODE EXFILTRATED -> [" + string(vdec_result_key) + "]");
            terminal_logs.push("[SYS]: KERNEL LOCKOUT DISENGAGED.");
        }
    } else if (crt_heat > 35.0) {
        crt_heat = vmath.clamp(crt_heat - (0.016 * 2.0), 35.0, 100.0);
    }

    # ================================================================
    # RENDER ENGINE
    # ================================================================
    vglib.begin();
    vglib.clear(COLOR_BLACK);

    # Calculate Jitter Offset
    jitter_x :: Float64 = (glitch_trigger > 0.0) ? (vmath.sin(run_time * 50.0) * (glitch_trigger * 10.0)) : 0.0;
    jitter_y :: Float64 = (glitch_trigger > 0.0) ? (vmath.cos(run_time * 30.0) * (glitch_trigger * 8.0)) : 0.0;

    # Background Panels
    vglib.rect(20 + jitter_x, 20 + jitter_y, 1240, 760, COLOR_PANEL);
    vglib.line(20, 20, 1260, 20, COLOR_BORDER);
    vglib.line(1260, 20, 1260, 780, COLOR_BORDER);
    vglib.line(1260, 780, 20, 780, COLOR_BORDER);
    vglib.line(20, 780, 20, 20, COLOR_BORDER);

    # Title Banner
    vglib.text_ex(vcr_font, "VYNE VEKTRAOS // VDEC CRACKING ENGINE", 50 + jitter_x, 45 + jitter_y, 16, COLOR_BLOOD);
    vglib.text_ex(vcr_font, "CRT TEMP: " + string(int64(crt_heat)) + "°C", 1080 + jitter_x, 45 + jitter_y, 12, (crt_heat > 75.0) ? COLOR_BLOOD : COLOR_TOXIC);

    vglib.line(40, 80, 1240, 80, COLOR_BORDER);

    # Terminal Log Box
    vglib.rect(40 + jitter_x, 100 + jitter_y, 1200, 560, COLOR_BLACK);
    vglib.line(40, 100, 1240, 100, COLOR_BORDER);
    vglib.line(1240, 100, 1240, 660, COLOR_BORDER);
    vglib.line(1240, 660, 40, 660, COLOR_BORDER);
    vglib.line(40, 660, 40, 100, COLOR_BORDER);

    through log_i :: 0..(terminal_logs.length() - 1) -> loop {
        log_y :: Float64 = 120.0 + (float64(log_i) * 24.0);
        if (log_y <= 630.0) {
            log_str :: String = string(terminal_logs[log_i]);
            log_col = COLOR_GHOST;
            if (log_str.find("[VDEC]") >= 0) { log_col = COLOR_AMBER; }
            if (log_str.find("[VDEC SUCCESS]") >= 0) { log_col = COLOR_TOXIC; }
            
            vglib.text_ex(vcr_font, log_str, 60 + jitter_x, log_y, 11, log_col);
        }
    };

    # Manual Start Button (Disabled during Lockout)
    btn_x :: Float64 = 40.0 + jitter_x;
    btn_y :: Float64 = 680.0 + jitter_y;
    btn_w :: Float64 = 320.0;
    btn_h :: Float64 = 45.0;

    btn_hover :: Int64 = (vdec_active == 0 && mx >= btn_x && mx <= (btn_x + btn_w) && my >= btn_y && my <= (btn_y + btn_h)) ? 1 : 0;
    
    if (btn_hover == 1 && m_click == 1) {
        vaudio.play_sound(click_sfx);
        trigger_vdec_crack(12, 4);
    }

    vglib.rect(btn_x, btn_y, btn_w, btn_h, (vdec_active == 1) ? COLOR_PANEL : (btn_hover == 1 ? COLOR_BLOOD : COLOR_BLACK));
    vglib.line(btn_x, btn_y, btn_x + btn_w, btn_y, COLOR_BLOOD);
    vglib.line(btn_x + btn_w, btn_y, btn_x + btn_w, btn_y + btn_h, COLOR_BLOOD);
    vglib.line(btn_x + btn_w, btn_y + btn_h, btn_x, btn_y + btn_h, COLOR_BLOOD);
    vglib.line(btn_x, btn_y + btn_h, btn_x, btn_y, COLOR_BLOOD);

    btn_label :: String = (vdec_active == 1) ? "[ LOCKED - DECODING... ]" : "EXECUTE VDEC DECODE";
    vglib.text_ex(vcr_font, btn_label, btn_x + 35.0, btn_y + 16.0, 11, (vdec_active == 1) ? COLOR_GHOST : (btn_hover == 1 ? COLOR_BLACK : COLOR_TOXIC));

    # Success Key Display Overlay
    if (vdec_success_flash > 0.0) {
        vglib.rect(400 + jitter_x, 680 + jitter_y, 480, 45, COLOR_BLACK);
        vglib.line(400, 680, 880, 680, COLOR_TOXIC);
        vglib.line(880, 680, 880, 725, COLOR_TOXIC);
        vglib.line(880, 725, 400, 725, COLOR_TOXIC);
        vglib.line(400, 725, 400, 680, COLOR_TOXIC);

        vglib.text_ex(vcr_font, "UNLOCKED KEY: " + string(vdec_result_key), 520 + jitter_x, 695, 14, COLOR_TOXIC);
    }

    # ================================================================
    # FULL-SCREEN VDEC CRACKING LOCKDOWN OVERLAY
    # ================================================================
    if (vdec_active == 1) {
        # Translucent Lockout Backdrop
        vglib.rect(0, 0, 1280, 800, vglib.rgba(4, 2, 6, 220));

        center_x :: Float64 = 640.0 + jitter_x;
        center_y :: Float64 = 400.0 + jitter_y;

        # Chassis Window
        win_w :: Float64 = 720.0;
        win_h :: Float64 = 360.0;
        win_x :: Float64 = center_x - (win_w / 2.0);
        win_y :: Float64 = center_y - (win_h / 2.0);

        vglib.rect(win_x, win_y, win_w, win_h, COLOR_BLACK);
        vglib.line(win_x, win_y, win_x + win_w, win_y, COLOR_BLOOD);
        vglib.line(win_x + win_w, win_y, win_x + win_w, win_y + win_h, COLOR_BLOOD);
        vglib.line(win_x + win_w, win_y + win_h, win_x, win_y + win_h, COLOR_BLOOD);
        vglib.line(win_x, win_y + win_h, win_x, win_y, COLOR_BLOOD);

        # Corner Accents
        c_len :: Float64 = 20.0;
        vglib.rect(win_x - 4.0, win_y - 4.0, c_len, 4.0, COLOR_AMBER);
        vglib.rect(win_x - 4.0, win_y - 4.0, 4.0, c_len, COLOR_AMBER);
        vglib.rect(win_x + win_w - c_len + 4.0, win_y - 4.0, c_len, 4.0, COLOR_AMBER);
        vglib.rect(win_x + win_w, win_y - 4.0, 4.0, c_len, COLOR_AMBER);

        # Header Title
        vglib.text_ex(vcr_font, "[ CRITICAL RING-0 DECRYPTION LOCKOUT ]", win_x + 120.0, win_y + 30.0, 14, COLOR_BLOOD);
        vglib.text_ex(vcr_font, "INPUT FROZEN -- BRUTE-FORCING SECTOR REGISTER 0x" + string(vdec_raw_payload), win_x + 80.0, win_y + 60.0, 10, COLOR_CYAN);

        vglib.line(win_x + 20.0, win_y + 85.0, win_x + win_w - 20.0, win_y + 85.0, COLOR_BORDER);

        # Animated Hex Stream Matrix
        through row :: 0..3 -> loop {
            row_y :: Float64 = win_y + 105.0 + float64(row * 22);
            hex_offset :: Int64 = int64(vmath.fmod(run_time * 80.0 + float64(row * 43), 99999.0));
            rand_hex :: String = "0x" + string(hex_offset) + " -> " + string(int64(vmath.random(1000, 9999))) + " " + string(int64(vmath.random(1000, 9999))) + " " + string(int64(vmath.random(1000, 9999)));
            
            vglib.text_ex(vcr_font, rand_hex, win_x + 130.0, row_y, 11, COLOR_AMBER);
        };

        # Progress Calculation
        progress_ratio :: Float64 = vmath.clamp(vdec_timer / vdec_duration, 0.0, 1.0);
        pct_int        :: Int64   = int64(progress_ratio * 100.0);

        # Progress Bar Container
        bar_x :: Float64 = win_x + 60.0;
        bar_y :: Float64 = win_y + 220.0;
        bar_w :: Float64 = win_w - 120.0;
        bar_h :: Float64 = 24.0;

        vglib.rect(bar_x, bar_y, bar_w, bar_h, COLOR_PANEL);
        vglib.rect(bar_x, bar_y, bar_w * progress_ratio, bar_h, COLOR_TOXIC);

        vglib.line(bar_x, bar_y, bar_x + bar_w, bar_y, COLOR_BORDER);
        vglib.line(bar_x + bar_w, bar_y, bar_x + bar_w, bar_y + bar_h, COLOR_BORDER);
        vglib.line(bar_x + bar_w, bar_y + bar_h, bar_x, bar_y + bar_h, COLOR_BORDER);
        vglib.line(bar_x, bar_y + bar_h, bar_x, bar_y, COLOR_BORDER);

        # Progress Percentage Readout
        vglib.text_ex(vcr_font, "DECODING: " + string(pct_int) + "%", center_x - 60.0, bar_y + 5.0, 11, (progress_ratio > 0.5) ? COLOR_BLACK : COLOR_CYAN);

        # Warning Footer
        vglib.text_ex(vcr_font, "WARNING: DO NOT SEVER POWER OR DISCONNECT SOCKET", win_x + 115.0, win_y + 280.0, 10, COLOR_BLOOD);
        vglib.text_ex(vcr_font, "HEAT RESIDUAL INCREASING AT 4.5°C/s", win_x + 180.0, win_y + 305.0, 9, COLOR_AMBER);
    }

    # CRT Scanlines
    vglib.draw_scanlines(8.0, vglib.rgba(0, 0, 0, 90));

    vglib.end();
}