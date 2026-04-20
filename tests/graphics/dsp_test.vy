ruleset { dynamic_casting };
module vglib;
module vaudio;
module vmath;

vglib.init(1000, 600, 60, "Vyne Saturator - Full Mode", 0);
vaudio.init_audio();
vaudio.volume(1.0);

# Sound-u yükləyirik (Səndə işləyən metod)
track = vaudio.load_sound("tests/assets/akira.wav");
vaudio.attach_saturator(track);
vaudio.play_sound(track);

run_time = 0.0;
drive = 0.4;
mode = 0;

while (vglib.running()) {
    run_time = run_time + 0.016;
    
    m = vglib.mouse_pos();
    md = vglib.mouse_delta();

    if (vglib.mouse_down(vglib.MOUSE_LEFT)) {
        if (vmath.hypot(m[0] - 250, m[1] - 300) < 50) {
            drive = vmath.clamp(drive - (md[1] * 0.005), 0.0, 1.0);
        }
    }
    
    if (vglib.key_pressed(vglib.SPACE)) { mode = (mode + 1) % 2; }

    # Parametrləri göndər
    vaudio.set_dsp(drive, mode);

    vglib.begin();
        vglib.clear(vglib.rgba(20, 20, 25, 255));
        
        # Analyzer (Mütləq tərpənməlidir)
        through i :: 0..80 -> loop {
            x_pos = 100 + (i * 10);
            
            # vmath.sin və tanh-ın double qaytarması bəzən vglib-i çaşdırır
            # ona görə daxili riyaziyyatı float-a yaxın saxlayırıq
            wave = vmath.sin(run_time * 12.0 + i * 0.15);
            h = vmath.tanh(wave * (1.0 + drive * 10.0)) * 60.0;
            
            # Əgər h mənfidirsə, rect yuxarı çəkiləcək
            vglib.rect(x_pos, 500, 6, -h, vglib.rgba(255, 100, 50, 200));
        };
        
        vglib.text("ANALOG SATURATOR ACTIVE", 40, 40, 20, vglib.GREEN);
        vglib.text("DRIVE: " + string(vmath.round(drive * 100)) + "%", 210, 380, 18, vglib.RED);
        vglib.text("MODE: " + string(mode), 700, 550, 12, vglib.WHITE);
    vglib.end();
}