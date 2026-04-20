ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

# 1. Başlanğıc
vglib.init(1000, 600, 60, "Vyne Saturator Pro", 0);
vaudio.init_audio();
vaudio.volume(1.0);

# 2. Audio Stream (Yolun düzgünlüyündən əmin ol)
track = vaudio.play_stream("tests/assets/akira.wav");

run_time = 0.0;
drive = 0.3;
mode = 0;
modes = ["SOFT TUBE", "HARD CLIP", "ASYMMETRIC"];

# --- UI FUNKSİYASI ---
fn draw_knob(name, x, y, val, color) {
    vglib.circle(x, y, 42.0, vglib.BLACK);
    vglib.circle(x, y, 40.0, vglib.rgba(70, 70, 80, 255));
    
    angle = (val * 270.0) - 135.0;
    rad = vmath.radians(angle);
    
    line_x = x + vmath.sin(rad) * 35.0;
    line_y = y - vmath.cos(rad) * 35.0;
    
    vglib.line(x, y, line_x, line_y, color);
    vglib.circle(line_x, line_y, 4.0, color);
    
    vglib.text(name, x - 25, y + 55, 18, vglib.WHITE);
    vglib.text(string(vmath.round(val * 100)) + "%", x - 15, y - 5, 12, color);
}

while (vglib.running()) {
    run_time = run_time + 0.016;
    
    if (track != 0) {
        vaudio.update_stream(track);
    }
    
    m = vglib.mouse_pos();
    md = vglib.mouse_delta();

    # DRIVE CONTROL (Vertical Drag)
    if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
        if (vmath.hypot(m[0] - 250, m[1] - 300) < 50) {
            drive = vmath.clamp(drive - (md[1] * 0.005), 0.0, 1.0);
        }
    }
    
    if (vglib.key_pressed(vglib.SPACE)) { 
        mode = (mode + 1) % 3; 
    }

    # DSP-ni C++ tərəfinə ötür
    vaudio.set_dsp(drive, mode);

        vglib.begin();
            vglib.clear(vglib.rgba(25, 25, 30, 255));
            
            # UI
            draw_knob("DRIVE", 250, 300, drive, vglib.rgba(255, 100, 50, 255));
            
            # Visualizer
            through i :: 0..80 -> loop {
                x_p = 100 + (i * 10);
                # Vizualizasiya üçün drive-ı istifadə edirik
                h = vmath.tanh(vmath.sin(run_time * 10.0 + i * 0.1) * (1.0 + drive * 10.0)) * 60.0;
                vglib.rect(x_p, 500, 6, -h, vglib.rgba(255, 100, 50, 255));
            };
            
            # Display Info
            vglib.text("MODE: " + modes[mode], 700, 280, 20, vglib.GREEN);
            vglib.text("Vyne Audio Engine v0.0.1", 700, 550, 12, vglib.RED);
            
        vglib.end();
}

vaudio.close_audio();
vglib.close();