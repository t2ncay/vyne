use native vraylib;
ruleset { dynamic_casting };
module vraylib;

fn :: vraylib ambiance(path :: String, volume :: Float64) -> Int64 {
    if !vraylib.is_audio_device_ready() { vraylib.init_audio_device(); }
    if !vraylib.is_audio_device_ready() { return 0; }
    music :: Int64 := vraylib.load_music_stream(path);
    if !vraylib.is_music_valid(music) { vraylib.unload_music_stream(music); return 0; }
    vraylib.set_field(music, "looping", true);
    vraylib.set_music_volume(music, volume);
    vraylib.play_music_stream(music);
    return music;
}
