use external "vraylib/vraylib.vy";
ruleset {
    dynamic_casting
};

# Read an integer environment setting, using the fallback when the option is absent or empty.
#
# Parameters:
#     name (String): Display/resource/uniform name or environment-variable key, according to this API.
#     fallback (Int64): Value used when the environment option is missing or empty.
#
# Returns:
#     The Int64 value selected by the conditions described above.
fn vmge_setting(name :: String, fallback :: Int64) -> Int64 {
    value := vraylib.environment(name);
    if value == null {
        return fallback;
    }
    return int64(value);
}

# Read a string environment option, using the fallback when the option is absent or empty.
#
# Parameters:
#     name (String): Display/resource/uniform name or environment-variable key, according to this API.
#     fallback (String): Value used when the environment option is missing or empty.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn vmge_option(name :: String, fallback :: String) -> String {
    value := vraylib.environment(name);
    if value == null {
        return fallback;
    }
    return value;
}

# Discover the VMGE project directory from an explicit root, working directory, or executable
# location.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
#
# Errors:
#     VMGE project root not found (with contextual detail).
#     VMGE project directory not found; set VYNE_VMGE_ROOT.
fn vmge_project_root() -> String {
    override := vraylib.environment("VYNE_VMGE_ROOT");
    if override != null {
        if !vraylib.file_exists(override + "/project.toml") {
            throw "VMGE project root not found: " + override;
        }
        return override;
    }
    directory :: String := vraylib.get_application_directory();
    roots := [
        "examples/projects/vmge",
        ".",
        directory + "..",
        directory + "../..",
        directory + "../examples/projects/vmge",
        directory + "../../examples/projects/vmge"
    ];
    through root :: roots -> loop {
        if vraylib.file_exists(root + "/shaders/fog.fs") && vraylib.file_exists(root + "/project.toml") {
            return root;
        }
    };
    throw "VMGE project directory not found; set VYNE_VMGE_ROOT";
}

# Resolve the original-media directory or select procedural assets according to the asset option.
#
# Parameters:
#     project (String): VMGE project directory containing shaders, assets, and configuration.
#
# Returns:
#     The resulting text/path. Returned strings are temporary within managed frames.
fn vmge_asset_root(project :: String) -> String {
    requested :: String := vmge_option("VYNE_VMGE_ASSETS", "auto");
    if requested == "procedural" {
        return "";
    }
    if requested != "auto" {
        return requested;
    }
    roots := ["examples/assets", project + "/../../assets"];
    through root :: roots -> loop {
        if vraylib.file_exists(root + "/building.jpg") {
            return root;
        }
    };
    return "";
}

# Return the eight historical texture paths in their persisted palette-slot order.
#
# Returns:
#     Array containing the resulting retained state or payload.
fn vmge_palette_paths() -> Array {
    return [
        "Brick/Brick_16-512x512.png",
        "wall.jpeg",
        "building.jpg",
        "yusif.jpeg",
        "asphalt_road_3.jpg",
        "Metal/Metal_18-512x512.png",
        "Metal/Metal_18-512x512.png",
        "Metal/Metal_18-512x512.png"
    ];
}

# Create and upload a tinted checkerboard texture owned by the application.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     tint (vraylib.Color): RGBA tint applied while drawing.
#
# Returns:
#     The computed vraylib.Texture value: texture.
fn vmge_checker(app :: vraylib.App, tint :: vraylib.Color) -> vraylib.Texture {
    image :: vraylib.Image := app.image(64, 64, tint);
    row :: Int64 := 0;
    while row < 4 {
        column :: Int64 := 0;
        while column < 4 {
            if (row + column) % 2 == 0 {
                vraylib.image_draw_rectangle(
                    vraylib.require_managed(image.session, image.token),
                    column * 16,
                    row * 16,
                    16,
                    16,
                    vraylib.DARKGRAY
                );
            }
            column = column + 1;
        }
        row = row + 1;
    }
    texture :: vraylib.Texture := app.texture(image);
    image.close();
    return texture;
}

# Load the shared eight-slot texture palette, reusing duplicate original textures or generating
# checkers.
#
# Parameters:
#     app (vraylib.App): Live managed application that owns the graphics context and native resources.
#     root (String): Filesystem/resource path used by the loading, saving, or resolution operation.
#
# Returns:
#     Array containing the resulting retained state or payload.
#
# Lifecycle:
#     Create retained session owners before entering managed frames; do not publish a frame-local
#     allocation into a longer-lived owner.
fn vmge_palette(app :: vraylib.App, root :: String) -> Array {
    textures :: Array := [];
    if root != "" {
        app.assets(root);
        through path :: vmge_palette_paths() -> loop {
            textures.push(app.load_texture(path));
        };
    } else {
        colors := [
            vraylib.MAROON,
            vraylib.LIGHTGRAY,
            vraylib.BEIGE,
            vraylib.PURPLE,
            vraylib.DARKGRAY,
            vraylib.BLUE,
            vraylib.GREEN,
            vraylib.ORANGE
        ];
        through color :: colors -> loop {
            textures.push(vmge_checker(app, color));
        };
    }
    return textures;
}
