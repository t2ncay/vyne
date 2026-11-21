use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };

fn vmge_setting(name :: String, fallback :: Int64) -> Int64 {
    value := vraylib.environment(name);
    if value == null { return fallback; }
    return int64(value);
}
fn vmge_option(name :: String, fallback :: String) -> String {
    value := vraylib.environment(name);
    if value == null { return fallback; }
    return value;
}

fn vmge_project_root() -> String {
    override := vraylib.environment("VYNE_VMGE_ROOT");
    if override != null {
        if !vraylib.file_exists(override + "/project.toml") { throw "VMGE project root not found: " + override; }
        return override;
    }
    directory :: String := vraylib.get_application_directory();
    roots := ["examples/projects/vmge", ".", directory + "..", directory + "../..", directory + "../examples/projects/vmge", directory + "../../examples/projects/vmge"];
    through root :: roots -> loop {
        if vraylib.file_exists(root + "/shaders/fog.fs") && vraylib.file_exists(root + "/project.toml") { return root; }
    };
    throw "VMGE project directory not found; set VYNE_VMGE_ROOT";
}

fn vmge_asset_root(project :: String) -> String {
    requested :: String := vmge_option("VYNE_VMGE_ASSETS", "auto");
    if requested == "procedural" { return ""; }
    if requested != "auto" { return requested; }
    roots := ["examples/assets", project + "/../../assets"];
    through root :: roots -> loop { if vraylib.file_exists(root + "/building.jpg") { return root; } };
    return "";
}

fn vmge_palette_paths() -> Array {
    return ["Brick/Brick_16-512x512.png", "wall.jpeg", "building.jpg", "yusif.jpeg", "asphalt_road_3.jpg", "Metal/Metal_18-512x512.png", "Metal/Metal_18-512x512.png", "Metal/Metal_18-512x512.png"];
}

fn vmge_checker(app :: vraylib.App, tint :: vraylib.Color) -> vraylib.Texture {
    image :: vraylib.Image := app.image(64, 64, tint);
    row :: Int64 := 0;
    while row < 4 {
        column :: Int64 := 0;
        while column < 4 {
            if (row + column) % 2 == 0 { vraylib.image_draw_rectangle(vraylib.require_managed(image.session, image.token), column * 16, row * 16, 16, 16, vraylib.DARKGRAY); }
            column = column + 1;
        }
        row = row + 1;
    }
    texture :: vraylib.Texture := app.texture(image);
    image.close();
    return texture;
}

fn vmge_palette(app :: vraylib.App, root :: String) -> Array {
    textures :: Array := [];
    if root != "" {
        app.assets(root);
        through path :: vmge_palette_paths() -> loop { textures.push(app.load_texture(path)); };
    } else {
        colors := [vraylib.MAROON, vraylib.LIGHTGRAY, vraylib.BEIGE, vraylib.PURPLE, vraylib.DARKGRAY, vraylib.BLUE, vraylib.GREEN, vraylib.ORANGE];
        through color :: colors -> loop { textures.push(vmge_checker(app, color)); };
    }
    return textures;
}
