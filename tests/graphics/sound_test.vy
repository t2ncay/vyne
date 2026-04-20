ruleset { dynamic_casting };

module vaudio;
module vglib;

vglib.init(1920, 1080, 60, "Vyne Ultra - Fullscreen Mode", vglib.FULLSCREEN);
vaudio.init_audio();

volume :: Float64 = 1.0;

vaudio.volume(volume);

riff = vaudio.load_sound("tests/assets/tuncay.wav");

vaudio.sound_volume(riff, 2.0);

while (vglib.running()) {
    vglib.begin();
    vglib.clear(vglib.rgba(20, 20, 20, 255));
    
    if (vglib.key_pressed(32)) {
        vaudio.play_sound(riff);
    }
    
    vglib.text("Music Volume: 200%", 220, 280, 40, vglib.rgba(0, 255, 150, 255));
    vglib.end();
}

vaudio.close_audio();
vglib.close();