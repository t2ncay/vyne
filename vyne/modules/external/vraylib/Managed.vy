# Hybrid application facade. Wrappers are value tokens; native state belongs to
# the application session, independently of the current frame's arena.
use "Types.vy";
use "Cameras.vy";
use "Models.vy";
use "Window.vy";
use "Audio.vy";
use native vraylib;
ruleset { dynamic_casting };
module vraylib;

fn :: vraylib require_managed(session :: Int64, token :: Int64) -> Int64 {
    if !vraylib.managed_valid(session, token) || token == 0 {
        throw "Resource is closed or belongs to another application";
    }
    return token;
}

interface Texture {
    token :: Int64,
    session :: Int64,
    owned :: Bool,
    valid() -> Bool { return vraylib.managed_valid(self.session, self.token) && self.token != 0; }
    close() -> Bool {
        if !self.owned { return false; }
        return vraylib.managed_release(self.session, self.token);
    }
    width() -> Int64 { return vraylib.managed_texture_size(self.session, self.token, 0); }
    height() -> Int64 { return vraylib.managed_texture_size(self.session, self.token, 1); }
    draw(position :: vraylib.Vector2, tint :: vraylib.Color) -> Bool {
        return vraylib.managed_draw_texture(self.session, self.token, position, tint);
    }
    native_handle() -> Int64 { return vraylib.managed_texture(self.session, self.token); }
}

interface Image {
    token :: Int64,
    session :: Int64,
    valid() -> Bool { return vraylib.managed_valid(self.session, self.token) && self.token != 0; }
    close() -> Bool { return vraylib.managed_release(self.session, self.token); }
    width() -> Int64 { return vraylib.field(vraylib.require_managed(self.session, self.token), "width", 0); }
    height() -> Int64 { return vraylib.field(vraylib.require_managed(self.session, self.token), "height", 0); }
    resize(width :: Int64, height :: Int64) -> Bool {
        if width < 1 || height < 1 { throw "Image dimensions must be positive"; }
        vraylib.image_resize(vraylib.require_managed(self.session, self.token), width, height);
        return true;
    }
    save(path :: String) -> Bool { return vraylib.export_image(vraylib.require_managed(self.session, self.token), path); }
}

interface Font {
    token :: Int64,
    session :: Int64,
    valid() -> Bool { return vraylib.managed_valid(self.session, self.token) && self.token != 0; }
    close() -> Bool { return vraylib.managed_release(self.session, self.token); }
    draw(text :: String, position :: vraylib.Vector2, size :: Float64, tint :: vraylib.Color) -> Bool {
        vraylib.draw_text_ex(vraylib.require_managed(self.session, self.token), text, position, size, 1.0, tint);
        return true;
    }
}

interface Shader {
    token :: Int64,
    session :: Int64,
    valid() -> Bool { return vraylib.managed_valid(self.session, self.token) && self.token != 0; }
    close() -> Bool { return vraylib.managed_release(self.session, self.token); }
    set(name :: String, value) -> Bool { return vraylib.managed_uniform(self.session, self.token, name, value, 0); }
    set_ints(name :: String, values :: Array<Int64>) -> Bool { return vraylib.managed_uniform(self.session, self.token, name, values, 1); }
    set_texture(name :: String, texture :: vraylib.Texture) -> Bool {
        if texture.session != self.session { throw "Sampler texture belongs to another application"; }
        return vraylib.managed_uniform(self.session, self.token, name, texture.token, 2);
    }
    begin() -> Bool { return vraylib.managed_pass(self.session, 3, self.token, true); }
    end() -> Bool { return vraylib.managed_pass(self.session, 3, self.token, false); }
    with_draw(draw) -> Bool {
        self.begin();
        try { draw(); } finally { self.end(); }
        return true;
    }
}

interface Camera {
    token :: Int64,
    session :: Int64,
    speed :: Float64,
    sensitivity :: Float64,
    controller :: Int64,
    valid() -> Bool { return vraylib.managed_valid(self.session, self.token) && self.token != 0; }
    close() -> Bool { return vraylib.managed_release(self.session, self.token); }
    position() -> vraylib.Vector3 { return vraylib.camera_position(vraylib.require_managed(self.session, self.token)); }
    set_position(position :: vraylib.Vector3) -> Bool { return vraylib.camera_set_position(vraylib.require_managed(self.session, self.token), position); }
    set_height(height :: Float64) -> Bool { return vraylib.camera_set_height(vraylib.require_managed(self.session, self.token), height); }
    move(forward :: Float64, right :: Float64) -> Bool { return vraylib.camera_step(vraylib.require_managed(self.session, self.token), forward, right); }
    look() -> Bool { return vraylib.camera_mouse_look(vraylib.require_managed(self.session, self.token), self.sensitivity); }
    update() -> Bool {
        handle :: Int64 := vraylib.require_managed(self.session, self.token);
        if self.controller == vraylib.CAMERA_ORBITAL { vraylib.update_camera(handle, vraylib.CAMERA_ORBITAL); return true; }
        self.look();
        forward :: Float64 := 0.0;
        right :: Float64 := 0.0;
        if vraylib.is_key_down(vraylib.KEY_W) { forward = forward + 1.0; }
        if vraylib.is_key_down(vraylib.KEY_S) { forward = forward - 1.0; }
        if vraylib.is_key_down(vraylib.KEY_D) { right = right + 1.0; }
        if vraylib.is_key_down(vraylib.KEY_A) { right = right - 1.0; }
        if forward != 0.0 && right != 0.0 { forward = forward * 0.70710678; right = right * 0.70710678; }
        dt :: Float64 := vraylib.managed_state(self.session, "dt");
        return self.move(forward * self.speed * dt, right * self.speed * dt);
    }
    begin() -> Bool { return vraylib.managed_pass(self.session, 2, self.token, true); }
    end() -> Bool { return vraylib.managed_pass(self.session, 2, self.token, false); }
    with_draw(draw) -> Bool { self.begin(); try { draw(); } finally { self.end(); } return true; }
}

interface Model {
    token :: Int64,
    session :: Int64,
    valid() -> Bool { return vraylib.managed_valid(self.session, self.token) && self.token != 0; }
    close() -> Bool { return vraylib.managed_release(self.session, self.token); }
    materials() -> Int64 { return vraylib.field(vraylib.require_managed(self.session, self.token), "materialCount", 0); }
    set_shader(shader :: vraylib.Shader) -> Bool {
        if shader.session != self.session { throw "Shader belongs to another application"; }
        vraylib.model_shader(vraylib.require_managed(self.session, self.token), vraylib.require_managed(shader.session, shader.token));
        return true;
    }
    set_texture(texture :: vraylib.Texture, material :: Int64) -> Bool {
        if texture.session != self.session { throw "Texture belongs to another application"; }
        return vraylib.managed_model_texture(self.session, self.token, texture.token, material);
    }
    draw(position :: vraylib.Vector3, scale :: Float64, tint :: vraylib.Color) -> Bool {
        vraylib.draw_model(vraylib.require_managed(self.session, self.token), position, scale, tint);
        return true;
    }
    draw_scaled(position :: vraylib.Vector3, scale :: vraylib.Vector3, tint :: vraylib.Color) -> Bool {
        return vraylib.model_at(vraylib.require_managed(self.session, self.token), position, scale, tint);
    }
}

interface Music {
    token :: Int64,
    session :: Int64,
    valid() -> Bool { return self.token != 0 && vraylib.managed_valid(self.session, self.token); }
    close() -> Bool { return vraylib.managed_release(self.session, self.token); }
    play() -> Bool { if !self.valid() { return false; } vraylib.play_music_stream(self.token); return true; }
    pause() -> Bool { if !self.valid() { return false; } vraylib.pause_music_stream(self.token); return true; }
    volume(value :: Float64) -> Bool { if !self.valid() { return false; } vraylib.set_music_volume(self.token, value); return true; }
}

interface RenderTarget {
    token :: Int64,
    session :: Int64,
    valid() -> Bool { return vraylib.managed_valid(self.session, self.token) && self.token != 0; }
    close() -> Bool { return vraylib.managed_release(self.session, self.token); }
    width() -> Int64 { return vraylib.managed_texture_size(self.session, self.token, 0); }
    height() -> Int64 { return vraylib.managed_texture_size(self.session, self.token, 1); }
    texture() -> vraylib.Texture { return vraylib.Texture(self.token, self.session, false); }
    resize(width :: Int64, height :: Int64) -> Bool { return vraylib.managed_resize(self.session, self.token, width, height); }
    begin() -> Bool { return vraylib.managed_pass(self.session, 1, self.token, true); }
    end() -> Bool { return vraylib.managed_pass(self.session, 1, self.token, false); }
    present(width :: Float64, height :: Float64) -> Bool { return vraylib.managed_present(self.session, self.token, width, height); }
    with_draw(draw) -> Bool { self.begin(); try { draw(); } finally { self.end(); } return true; }
}

interface App {
    token :: Int64,
    valid() -> Bool { return vraylib.managed_valid(self.token, 0); }
    close() -> Bool { return vraylib.managed_close(self.token); }
    assets(root :: String) -> Bool { return vraylib.managed_config(self.token, "assets", root); }
    asset(path :: String) -> String { return vraylib.managed_path(self.token, path); }
    limit(frames :: Int64) -> Bool { return vraylib.managed_config(self.token, "limit", frames); }
    stop() -> Bool { return vraylib.managed_config(self.token, "stop", true); }
    fps(value :: Int64) -> Bool { return vraylib.managed_config(self.token, "fps", value); }
    dt() -> Float64 { return vraylib.managed_state(self.token, "dt"); }
    elapsed() -> Float64 { return vraylib.managed_state(self.token, "elapsed"); }
    frames() -> Int64 { return vraylib.managed_state(self.token, "frames"); }
    width() -> Int64 { return vraylib.managed_state(self.token, "width"); }
    height() -> Int64 { return vraylib.managed_state(self.token, "height"); }
    resources() -> Int64 { return vraylib.managed_state(self.token, "resources"); }
    passes() -> Int64 { return vraylib.managed_state(self.token, "passes"); }
    uniform_queries() -> Int64 { return vraylib.managed_state(self.token, "uniform_queries"); }
    window_ready() -> Bool { return vraylib.managed_state(self.token, "window"); }
    frame() -> Bool { return vraylib.managed_frame(self.token); }
    end_frame() -> Bool { return vraylib.managed_finish(self.token); }
    begin() -> Bool { return vraylib.managed_pass(self.token, 0, 0, true); }
    end() -> Bool { return vraylib.managed_pass(self.token, 0, 0, false); }
    clear(color :: vraylib.Color) -> Bool { vraylib.clear_background(color); return true; }
    capture_mouse(capture :: Bool) -> Bool { if capture { vraylib.disable_cursor(); } else { vraylib.enable_cursor(); } return true; }
    camera(position :: vraylib.Vector3, target :: vraylib.Vector3, fovy :: Float64) -> vraylib.Camera {
        handle :: Int64 := vraylib.camera_create(position, target, fovy);
        vraylib.managed_adopt(self.token, handle);
        return vraylib.Camera(handle, self.token, 9.0, 0.15, vraylib.CAMERA_FIRST_PERSON);
    }
    first_person() -> vraylib.Camera {
        self.capture_mouse(true);
        return self.camera(vraylib.vec3(0.0, 1.8, 0.0), vraylib.vec3(0.0, 1.8, -1.0), 60.0);
    }
    orbit(position :: vraylib.Vector3, target :: vraylib.Vector3) -> vraylib.Camera {
        handle :: Int64 := vraylib.camera_create(position, target, 60.0);
        vraylib.managed_adopt(self.token, handle);
        return vraylib.Camera(handle, self.token, 0.0, 0.0, vraylib.CAMERA_ORBITAL);
    }
    target(width :: Int64, height :: Int64) -> vraylib.RenderTarget {
        return vraylib.RenderTarget(vraylib.managed_target(self.token, width, height, false), self.token);
    }
    screen_target() -> vraylib.RenderTarget {
        return vraylib.RenderTarget(vraylib.managed_target(self.token, self.width(), self.height(), true), self.token);
    }
    image(width :: Int64, height :: Int64, color :: vraylib.Color) -> vraylib.Image {
        return vraylib.Image(vraylib.managed_image(self.token, width, height, color), self.token);
    }
    load_image(path :: String) -> vraylib.Image {
        resolved :: String := self.asset(path);
        handle :: Int64 := vraylib.managed_cache(self.token, resolved, "Image");
        if handle == 0 {
            handle = vraylib.load_image(resolved);
            vraylib.managed_adopt(self.token, handle);
            if !vraylib.is_image_valid(handle) { vraylib.managed_release(self.token, handle); throw "Could not load image: " + resolved; }
            vraylib.managed_store(self.token, resolved, "Image", handle);
        }
        return vraylib.Image(handle, self.token);
    }
    load_texture(path :: String) -> vraylib.Texture {
        if !self.window_ready() { throw "Textures require an application window"; }
        resolved :: String := self.asset(path);
        handle :: Int64 := vraylib.managed_cache(self.token, resolved, "Texture");
        if handle == 0 {
            handle = vraylib.load_texture(resolved);
            vraylib.managed_adopt(self.token, handle);
            if !vraylib.is_texture_valid(handle) { vraylib.managed_release(self.token, handle); throw "Could not load texture: " + resolved; }
            vraylib.managed_store(self.token, resolved, "Texture", handle);
        }
        return vraylib.Texture(handle, self.token, true);
    }
    load_model(path :: String) -> vraylib.Model {
        if !self.window_ready() { throw "Models require an application window"; }
        resolved :: String := self.asset(path);
        handle :: Int64 := vraylib.managed_cache(self.token, resolved, "Model");
        if handle == 0 {
            if !vraylib.file_exists(resolved) { throw "Model file not found: " + resolved; }
            handle = vraylib.load_model(resolved);
            vraylib.managed_adopt(self.token, handle);
            if !vraylib.is_model_valid(handle) { vraylib.managed_release(self.token, handle); throw "Could not load model: " + resolved; }
            vraylib.managed_store(self.token, resolved, "Model", handle);
        }
        return vraylib.Model(handle, self.token);
    }
    load_font(path :: String) -> vraylib.Font {
        if !self.window_ready() { throw "Fonts require an application window"; }
        resolved :: String := self.asset(path);
        handle :: Int64 := vraylib.managed_cache(self.token, resolved, "Font");
        if handle == 0 {
            if !vraylib.file_exists(resolved) { throw "Font file not found: " + resolved; }
            handle = vraylib.load_font(resolved);
            vraylib.managed_adopt(self.token, handle);
            if !vraylib.is_font_valid(handle) { vraylib.managed_release(self.token, handle); throw "Could not load font: " + resolved; }
            vraylib.managed_store(self.token, resolved, "Font", handle);
        }
        return vraylib.Font(handle, self.token);
    }
    load_shader(vertex :: String, fragment :: String) -> vraylib.Shader {
        if !self.window_ready() { throw "Shaders require an application window"; }
        vertex_path := vertex;
        if vertex != null { vertex_path = self.asset(vertex); if !vraylib.file_exists(vertex_path) { throw "Vertex shader file not found: " + vertex_path; } }
        fragment_path :: String := self.asset(fragment);
        if !vraylib.file_exists(fragment_path) { throw "Fragment shader file not found: " + fragment_path; }
        key :: String := "default:" + fragment_path;
        if vertex_path != null { key = "vertex" + string(vertex_path.size()) + ":" + vertex_path + ":" + fragment_path; }
        handle :: Int64 := vraylib.managed_cache(self.token, key, "Shader");
        if handle == 0 {
            handle = vraylib.load_shader(vertex_path, fragment_path);
            vraylib.managed_adopt(self.token, handle);
            if !vraylib.is_shader_valid(handle) || vraylib.field(handle, "id", 0) == vraylib.gl_get_shader_id_default() {
                vraylib.managed_release(self.token, handle); throw "Custom shader compilation failed: " + key;
            }
            vraylib.managed_store(self.token, key, "Shader", handle);
        }
        return vraylib.Shader(handle, self.token);
    }
    cube(texture :: vraylib.Texture) -> vraylib.Model {
        if texture.session != self.token { throw "Texture belongs to another application"; }
        handle :: Int64 := vraylib.textured_cube(texture.native_handle());
        vraylib.managed_adopt(self.token, handle);
        result :: vraylib.Model := vraylib.Model(handle, self.token);
        result.set_texture(texture, 0);
        return result;
    }
    plane(texture :: vraylib.Texture, width :: Float64, length :: Float64) -> vraylib.Model {
        if texture.session != self.token { throw "Texture belongs to another application"; }
        handle :: Int64 := vraylib.textured_plane(texture.native_handle(), width, length);
        vraylib.managed_adopt(self.token, handle);
        result :: vraylib.Model := vraylib.Model(handle, self.token);
        result.set_texture(texture, 0);
        return result;
    }
    music(path :: String, volume :: Float64) -> vraylib.Music {
        resolved :: String := self.asset(path);
        if !vraylib.file_exists(resolved) { throw "Music file not found: " + resolved; }
        handle :: Int64 := vraylib.managed_cache(self.token, resolved, "Music");
        if handle == 0 {
            handle = vraylib.ambiance(resolved, volume);
            if handle != 0 { vraylib.managed_store(self.token, resolved, "Music", handle); }
        }
        return vraylib.Music(handle, self.token);
    }
    run(update, draw) -> Int64 {
        total :: Int64 := 0;
        running :: Bool := true;
        try {
            while running {
                if !self.valid() { running = false; }
                else {
                    if !self.frame() { running = false; }
                    else {
                        total = self.frames();
                        update(self);
                        if self.valid() {
                            if self.window_ready() {
                                self.begin(); draw(self);
                                if self.valid() { self.end(); }
                            } else { draw(self); }
                        }
                    }
                }
            }
            if self.valid() { total = self.frames(); }
        } catch (error) { self.close(); throw error; }
        self.close();
        return total;
    }
}

fn :: vraylib session(asset_root :: String) -> vraylib.App {
    return vraylib.App(vraylib.managed_session(asset_root));
}
fn :: vraylib app(width :: Int64, height :: Int64, title :: String) -> vraylib.App {
    return vraylib.app_options(width, height, title, 60, vraylib.FLAG_WINDOW_RESIZABLE + vraylib.FLAG_VSYNC_HINT);
}
fn :: vraylib app_options(width :: Int64, height :: Int64, title :: String, fps :: Int64, flags :: Int64) -> vraylib.App {
    owner :: vraylib.App := vraylib.session("");
    try { vraylib.managed_open(owner.token, width, height, title, fps, flags); }
    catch (error) { owner.close(); throw error; }
    return owner;
}
