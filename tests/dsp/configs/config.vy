ruleset { dynamic_casting };

module configs;

group :: configs Fonts {
    vcr_mono = "tests/assets/VCR_OSD_MONO_1.001.ttf";
};

group :: configs Audios {
    cigerlerim      :: String = "tests/assets/cigerlerim.mp3";
    breakbeats      :: String = "tests/assets/Breakbeats.wav";
    osamason_1300   :: String = "tests/assets/1300_sk_wh.mp3";
    ariana_bunny    :: String = "tests/assets/buni.mp3";
    Breakbeats      :: String = "tests/assets/Breakbeats.mp3";
    never_fade_away :: String = "tests/assets/Never Fade Away.mp3";
    ikit1bb           :: String = "tests/assets/2t1bb.mp3";
    eternal_sunshine  :: String = "tests/assets/eternal_sunshine.mp3";
};

deploy configs;