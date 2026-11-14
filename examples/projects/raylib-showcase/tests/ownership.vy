use external "vraylib/vraylib.vy";
ruleset { dynamic_casting };

fn check(ok :: Bool, message :: String) -> Bool {
    if !ok { throw message; }
    return true;
}

count_storage := vraylib.buffer("int", 1);
codes := vraylib.load_codepoints("Vyne", count_storage);
check(vraylib.buffer_read(count_storage, 0) == 4, "scalar output pointer");
check(vraylib.buffer_read(codes, 0) == 86, "returned typed allocation");
vraylib.unload_codepoints(codes);
vraylib.release(count_storage);

data := vraylib.bytes([0, 1, 2, 255]);
size_storage := vraylib.buffer("int", 1);
encoded := vraylib.encode_data_base64(data, 4, size_storage);
decoded := vraylib.decode_data_base64(encoded, size_storage);
check(vraylib.buffer_read(size_storage, 0) == 4, "base64 output length");
check(vraylib.buffer_read(decoded, 3) == 255, "binary buffer roundtrip");
vraylib.mem_free(encoded);
vraylib.mem_free(decoded);
vraylib.release(data);
vraylib.release(size_storage);

failed :: Bool := false;
handle := vraylib.buffer("float", 1);
try { vraylib.buffer_read(handle, 1); } catch (error) { failed = true; }
check(failed, "bounds error must be catchable");
failed = false;
vraylib.release(handle);
try { vraylib.buffer_read(handle, 0); } catch (error) { failed = true; }
check(failed, "stale handle rejected");

failed = false;
image := vraylib.gen_image_color(2, 2, vraylib.BLUE);
try { vraylib.unload_texture(image); } catch (error) { failed = true; }
check(failed, "wrong unload rejected");
vraylib.unload_image(image);

failed = false;
try { vraylib.draw_line_strip([vraylib.vec2(0.0, 0.0)], 3, vraylib.RED); } catch (error) { failed = true; }
check(failed, "pointer length validated before native drawing");

point := vraylib.buffer("Vector2", 1);
check(vraylib.check_collision_lines(vraylib.vec2(0.0, 0.0), vraylib.vec2(2.0, 2.0), vraylib.vec2(0.0, 2.0), vraylib.vec2(2.0, 0.0), point), "collision output pointer");
intersection := vraylib.buffer_read(point, 0);
check(intersection.x == 1.0 && intersection.y == 1.0, "collision output value");
vraylib.release(point);

text := vraylib.text_replace("hello old world", "old", "new");
check(text == "hello new world", "static returned text is copied");
vraylib.shutdown();
check(vraylib.live_handles() == 0, "no native objects remain");
out("VRAYLIB_OWNERSHIP_OK");
