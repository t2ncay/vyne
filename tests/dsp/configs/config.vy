ruleset { dynamic_casting };

module configs;

group :: configs Fonts {
    vcr_mono = "tests/assets/VCR_OSD_MONO_1.001.ttf";
};

group :: configs Audios {
    cigerlerim = "tests/assets/cigerlerim.mp3";
    breakbeats = "tests/assets/Breakbeats.wav";
    osamason_1300 = "tests/assets/1300_sk_wh.mp3";
    ariana_bunny = "tests/assets/buni.mp3";
    Breakbeats = "tests/assets/Breakbeats.mp3";
    never_fade_away = "tests/assets/Never Fade Away.mp3";
};

deploy configs;