use external "vraylib/vraylib.vy";
use native vraylib as rl;
use native vmem;
ruleset { dynamic_casting };

fn check(ok :: Bool, message :: String) -> Bool {
    if !ok { throw message; }
    return true;
}

v := rl.math_vector3_add(vraylib.vec3(1.0, 2.0, 3.0), vraylib.vec3(4.0, 5.0, 6.0));
check(v.x == 5.0 && v.y == 7.0 && v.z == 9.0, "raymath and returned field ids");
check(rl.KEY_SPACE == 32, "constant getter");
check(rl.check_collision_recs(vraylib.Rectangle(0.0, 0.0, 2.0, 2.0), vraylib.Rectangle(1.0, 1.0, 2.0, 2.0)), "rectangle conversion");
handle :: Int64 := 0;
region allocate {
    handle = rl.buffer("float", 4);
    rl.buffer_write(handle, 0, 3.5);
};
check(rl.buffer_read(handle, 0) == 3.5, "buffer survives region rewind");
rl.release(handle);
check(!rl.valid(handle), "released handle is invalid");
camera := vraylib.camera_create(vraylib.vec3(0.0, 1.8, 0.0), vraylib.vec3(0.0, 1.8, -1.0), 80.0);
vraylib.camera_step(camera, 2.0, 0.0);
position := vraylib.camera_position(camera);
check(position.z == -2.0, "camera movement");
rl.release(camera);
image := rl.gen_image_color(8, 8, rl.RED);
check(rl.field(image, "width", 0) == 8, "image metadata");
rl.image_resize(image, 16, 16);
check(rl.field(image, "width", 0) == 16, "mutable Image pointer");
rl.unload_image(image);
rl.shutdown();
check(rl.live_handles() == 0, "all handles cleaned");
out("VRAYLIB_API_OK");
