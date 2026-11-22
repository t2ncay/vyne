use "../scene/text.vy";
use native vfs;

# Only resource IDs survive frames. Thin Font wrappers are reconstructed locally.
se_theme_tokens :: Array<Int64> := [0, 0, 0, 0, 0, 1, 0, 0];
se_theme_metrics :: Array<Float64> := [0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
# Workspace tokens are separate; the scene's six legacy metrics keep their ABI.
ss_theme_resources :: Array<Int64> := [0, 0];
ss_theme_scale :: Array<Float64> := [1.0];
ss_theme_input :: Array<Int64> := [1];

# Convert a logical UI dimension into DPI-scaled pixels using the shared theme scale.
#
# Parameters:
#     value (Float64): Candidate property/parameter value or display text, as typed below.
#
# Returns:
#     The computed Float64 value: value * ss_theme_scale[0].
fn ss_scale(value :: Float64) -> Float64 {
    return value * ss_theme_scale[0];
}

# Clamp and set the shared UI scale from 0.75 through 3.
#
# Parameters:
#     scale (Float64): Logical-to-pixel UI scale, clamped to 0.75 through 3.
#
# Returns:
#     True when the operation completes without throwing.
fn ss_theme_dpi(scale :: Float64) -> Bool {
    ss_theme_scale[0] = vraylib.math_clamp(scale, 0.75, 3.0);
    return true;
}

# Return the DPI-scaled monospace editor font size.
#
# Returns:
#     The computed Int64 value: int64(ss_scale(14.0)).
fn ss_font_size() -> Int64 {
    return int64(ss_scale(14.0));
}

# Return the DPI-scaled source-editor line height.
#
# Returns:
#     The computed Float64 value: ss_scale(22.0).
fn ss_line_height() -> Float64 {
    return ss_scale(22.0);
}

# Measure a monospace glyph with the loaded font, using a scaled fallback before font initialization.
#
# Returns:
#     The Float64 value selected by the conditions described above.
fn ss_cell_width() -> Float64 {
    if ss_theme_resources[0] == 0 {
        return ss_scale(8.4);
    }
    font :: vraylib.Font := vraylib.Font(ss_theme_resources[0], ss_theme_resources[1]);
    return font.measure("M", float64(ss_font_size())).x;
}

# Draw source/diagnostic text using the loaded monospace font or the default font fallback.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#     x (Float64): Screen-space X coordinate in pixels.
#     y (Float64): Screen-space Y coordinate in pixels.
#     tint (vraylib.Color): RGBA tint applied while drawing.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn ss_mono(text :: String, x :: Float64, y :: Float64, tint :: vraylib.Color) -> Bool {
    if ss_theme_resources[0] == 0 {
        return vraylib.draw_text(text, int64(x), int64(y), ss_font_size(), tint);
    }
    font :: vraylib.Font := vraylib.Font(ss_theme_resources[0], ss_theme_resources[1]);
    return font.draw(text, vraylib.vec2(x, y), float64(ss_font_size()), tint);
}

# Semantic palette: ink, surface, raised, border, text, muted, accent, error, success.

# Resolve a semantic UI palette role to its RGBA color.
#
# Parameters:
#     role (Int64): Semantic palette index: ink/surface/raised/border/text/muted/accent/error/success.
#
# Returns:
#     The vraylib.Color value selected by the conditions described above.
fn ss_color(role :: Int64) -> vraylib.Color {
    if role == 0 {
        return vraylib.rgba(17, 20, 26, 255);
    }
    if role == 1 {
        return vraylib.rgba(23, 27, 34, 255);
    }
    if role == 2 {
        return vraylib.rgba(32, 38, 47, 255);
    }
    if role == 3 {
        return vraylib.rgba(48, 57, 69, 255);
    }
    if role == 4 {
        return vraylib.rgba(222, 228, 237, 255);
    }
    if role == 5 {
        return vraylib.rgba(131, 146, 165, 255);
    }
    if role == 6 {
        return vraylib.rgba(112, 177, 233, 255);
    }
    if role == 7 {
        return vraylib.rgba(244, 128, 124, 255);
    }
    return vraylib.rgba(133, 199, 163, 255);
}

# Begin a managed panel clip and retain its rectangle so field scissors can restore it.
#
# Parameters:
#     clip (vraylib.Clip): Managed panel clip whose balanced begin/end brackets the UI drawing area.
#     rectangle (vraylib.Rectangle): Screen-space rectangle for drawing, layout, or pointer hit
#         testing.
#
# Returns:
#     True when the operation completes without throwing.
fn se_clip_start(clip :: vraylib.Clip, rectangle :: vraylib.Rectangle) -> Bool {
    clip.begin();
    se_theme_tokens[3] = 1;
    se_theme_metrics[2] = rectangle.x;
    se_theme_metrics[3] = rectangle.y;
    se_theme_metrics[4] = rectangle.width;
    se_theme_metrics[5] = rectangle.height;
    return true;
}

# End the managed panel clip and clear shared clip tracking.
#
# Parameters:
#     clip (vraylib.Clip): Managed panel clip whose balanced begin/end brackets the UI drawing area.
#
# Returns:
#     True when the operation completes without throwing.
fn se_clip_finish(clip :: vraylib.Clip) -> Bool {
    clip.end();
    se_theme_tokens[3] = 0;
    return true;
}

# Load retained Inter regular/semibold and JetBrains Mono managed font resources for the application
# session.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     project (String): VMGE project directory containing shaders, assets, and configuration.
#
# Returns:
#     True when the operation completes without throwing.
fn se_theme_load(app :: vraylib.App, project :: String) -> Bool {
    if se_theme_tokens[2] == app.token && se_theme_tokens[0] != 0 {
        return true;
    }
    regular :: vraylib.Font := app.load_font_size(vfs.join(project, "assets/fonts/Inter-Regular.ttf"), 32);
    bold :: vraylib.Font := app.load_font_size(vfs.join(project, "assets/fonts/Inter-SemiBold.ttf"), 32);
    se_theme_tokens[0] = regular.token;
    se_theme_tokens[1] = bold.token;
    se_theme_tokens[2] = app.token;
    mono :: vraylib.Font := app.load_font_size(
        vfs.join(project, "assets/fonts/JetBrainsMono-Regular.ttf"),
        32
    );
    ss_theme_resources[0] = mono.token;
    ss_theme_resources[1] = app.token;
    return true;
}

# Draw proportional interface text using the loaded Inter font, falling back to raylib text drawing
# before initialization.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#     x (Int64): Screen-space X coordinate in pixels.
#     y (Int64): Screen-space Y coordinate in pixels.
#     size (Int64): Font size in pixels.
#     tint (vraylib.Color): RGBA tint applied while drawing.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn se_draw_text(text :: String, x :: Int64, y :: Int64, size :: Int64, tint :: vraylib.Color) -> Bool {
    if se_theme_tokens[0] == 0 {
        vraylib.draw_text(text, x, y, size, tint);
        return true;
    }
    font :: vraylib.Font := vraylib.Font(se_theme_tokens[0], se_theme_tokens[2]);
    return font.draw(text, vraylib.vec2(float64(x), float64(y)), float64(size), tint);
}

# Draw interface heading text using the semibold font, falling back to ordinary interface text.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#     x (Int64): Screen-space X coordinate in pixels.
#     y (Int64): Screen-space Y coordinate in pixels.
#     size (Int64): Font size in pixels.
#     tint (vraylib.Color): RGBA tint applied while drawing.
#
# Returns:
#     The Boolean outcome of this operation, including early no-op/failure paths described above.
fn se_draw_heading(text :: String, x :: Int64, y :: Int64, size :: Int64, tint :: vraylib.Color) -> Bool {
    if se_theme_tokens[1] == 0 {
        return se_draw_text(text, x, y, size, tint);
    }
    font :: vraylib.Font := vraylib.Font(se_theme_tokens[1], se_theme_tokens[2]);
    return font.draw(text, vraylib.vec2(float64(x), float64(y)), float64(size), tint);
}

# Measure interface text width using the loaded Unicode-aware font or raylib's fallback metrics.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#     size (Int64): Font size in pixels.
#
# Returns:
#     The Float64 value selected by the conditions described above.
fn se_measure(text :: String, size :: Int64) -> Float64 {
    if se_theme_tokens[0] == 0 {
        return float64(vraylib.measure_text(text, size));
    }
    font :: vraylib.Font := vraylib.Font(se_theme_tokens[0], se_theme_tokens[2]);
    return font.measure(text, float64(size)).x;
}

# Fit interface text into the available width, truncating safely at UTF-8 boundaries when necessary.
#
# Parameters:
#     text (String): Input text or retained UTF-8 buffer, as specified by the declared type.
#     width (Float64): Width or component count; rendering dimensions are measured in pixels.
#     size (Int64): Font size in pixels.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn se_fit(text :: String, width :: Float64, size :: Int64) -> String {
    if width <= 0.0 {
        return "";
    }
    if se_measure(text, size) <= width {
        return text;
    }
    count :: Int64 := vraylib.buffer("int", 1);
    points :: Int64 := 0;
    try {
        points = vraylib.load_codepoints(text, count);
        length :: Int64 := int64(vraylib.buffer_read(count, 0));
        bytes :: Int64 := text.size();
        while length > 0 {
            length = length - 1;
            bytes = bytes - se_utf8_length(int64(vraylib.buffer_read(points, length)));
            candidate :: String := vraylib.text_subtext(text, 0, bytes) + "...";
            if se_measure(candidate, size) <= width {
                return candidate;
            }
        }
    } finally {
        if points != 0 {
            vraylib.unload_codepoints(points);
        }
        vraylib.release(count);
    }
    return "";
}
