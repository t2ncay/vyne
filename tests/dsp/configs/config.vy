ruleset { dynamic_casting };

module configs;

group :: configs Fonts {
    vcr_mono = "tests/assets/VCR_OSD_MONO_1.001.ttf";
};

group :: configs Audios {
    cigerlerim = "tests/assets/cigerlerim.mp3";
    breakbeats = "tests/assets/Breakbeats.wav";
};

deploy configs;