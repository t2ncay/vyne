use "../scene/text.vy";
use native vfs;

# Only resource IDs survive frames. Thin Font wrappers are reconstructed locally.
se_theme_tokens :: Array<Int64> := [0,0,0,0,0,1,0,0];
se_theme_metrics :: Array<Float64> := [0.0,0.0,0.0,0.0,0.0,0.0];

fn se_clip_start(clip :: vraylib.Clip,rectangle :: vraylib.Rectangle) -> Bool {
    clip.begin(); se_theme_tokens[3] = 1;
    se_theme_metrics[2] = rectangle.x; se_theme_metrics[3] = rectangle.y; se_theme_metrics[4] = rectangle.width; se_theme_metrics[5] = rectangle.height; return true;
}
fn se_clip_finish(clip :: vraylib.Clip) -> Bool { clip.end(); se_theme_tokens[3] = 0; return true; }

fn se_theme_load(app :: vraylib.App,project :: String) -> Bool {
    if se_theme_tokens[2] == app.token && se_theme_tokens[0] != 0 { return true; }
    regular :: vraylib.Font := app.load_font_size(vfs.join(project,"assets/fonts/Inter-Regular.ttf"),32);
    bold :: vraylib.Font := app.load_font_size(vfs.join(project,"assets/fonts/Inter-SemiBold.ttf"),32);
    se_theme_tokens[0] = regular.token; se_theme_tokens[1] = bold.token; se_theme_tokens[2] = app.token;
    return true;
}
fn se_draw_text(text :: String,x :: Int64,y :: Int64,size :: Int64,tint :: vraylib.Color) -> Bool {
    if se_theme_tokens[0] == 0 { vraylib.draw_text(text,x,y,size,tint); return true; }
    font :: vraylib.Font := vraylib.Font(se_theme_tokens[0],se_theme_tokens[2]);
    return font.draw(text,vraylib.vec2(float64(x),float64(y)),float64(size),tint);
}
fn se_draw_heading(text :: String,x :: Int64,y :: Int64,size :: Int64,tint :: vraylib.Color) -> Bool {
    if se_theme_tokens[1] == 0 { return se_draw_text(text,x,y,size,tint); }
    font :: vraylib.Font := vraylib.Font(se_theme_tokens[1],se_theme_tokens[2]);
    return font.draw(text,vraylib.vec2(float64(x),float64(y)),float64(size),tint);
}
fn se_measure(text :: String,size :: Int64) -> Float64 {
    if se_theme_tokens[0] == 0 { return float64(vraylib.measure_text(text,size)); }
    font :: vraylib.Font := vraylib.Font(se_theme_tokens[0],se_theme_tokens[2]);
    return font.measure(text,float64(size)).x;
}
fn se_fit(text :: String,width :: Float64,size :: Int64) -> String {
    if width <= 0.0 { return ""; }
    if se_measure(text,size) <= width { return text; }
    count :: Int64 := vraylib.buffer("int",1); points :: Int64 := 0;
    try {
        points = vraylib.load_codepoints(text,count);
        length :: Int64 := int64(vraylib.buffer_read(count,0));
        bytes :: Int64 := text.size();
        while length > 0 {
            length = length-1; bytes = bytes-se_utf8_length(int64(vraylib.buffer_read(points,length)));
            candidate :: String := vraylib.text_subtext(text,0,bytes)+"...";
            if se_measure(candidate,size) <= width { return candidate; }
        }
    } finally { if points != 0 { vraylib.unload_codepoints(points); } vraylib.release(count); }
    return "";
}
