use "../src/engine/collision.vy";

fn vmge_check(ok :: Bool, message :: String) -> Bool { if !ok { throw message; } return true; }
app :: vraylib.App := vraylib.session("");
try {
    world :: VMGEMap := vmge_map_parse("2,1,0,2,0;", 16);
    camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0, 1.8, 0.0), vraylib.vec3(0.0, 1.8, 1.0), 80.0);
    failed :: Bool := false;
    try { camera.ray(vraylib.vec2(0.0, 0.0)); } catch (error) { failed = true; }
    vmge_check(failed && !app.drawing(), "headless picking reports missing window");
    vmge_slide(camera, world, 1.0, -3.0, 1.8);
    position :: vraylib.Vector3 := camera.position();
    vmge_check(position.x < 0.61 && position.z > 0.9, "slide along wall without sprint tunneling");
    vmge_check(!vmge_blocked(world, position, 1.8), "accepted movement is collision-free");
    camera.set_position(vraylib.vec3(2.0, 3.8, 0.0));
    vmge_check(vmge_floor(world, camera.position(), 1.8) == 2.0, "cube top supports player");
    vmge_check(!vmge_blocked(world, camera.position(), 1.8), "support face permits horizontal movement");
    empty :: VMGEMap := vmge_map_create(16);
    player :: VMGEPlayer := vmge_player(app);
    vmge_player_update(player, empty, 0.0, 0.0, false, true, false, 0.02);
    vmge_check(player.height() == 0.9 && vmath.abs(player.camera.position().y - 0.9) < 0.001, "crouch preserves feet");
    ceiling :: VMGEMap := vmge_map_parse("0,2.5,0,2,0;", 16);
    vmge_player_update(player, ceiling, 0.0, 0.0, false, false, false, 0.02);
    vmge_check(player.height() == 0.9, "cannot stand through ceiling");
    player.reset();
    vmge_player_update(player, empty, 0.0, 0.0, false, false, true, 0.02);
    vmge_check(!player.grounded() && player.camera.position().y > 1.8, "grounded jump starts ascent");
    before :: Float64 := float64(player.state[1]);
    vmge_player_update(player, empty, 0.0, 0.0, false, false, true, 0.02);
    vmge_check(float64(player.state[1]) < before, "airborne jump cannot reset velocity");
    i :: Int64 := 0;
    while i < 80 { vmge_player_update(player, empty, 0.0, 0.0, false, false, false, 0.02); i = i + 1; }
    vmge_check(player.grounded() && vmath.abs(player.camera.position().y - 1.8) < 0.001, "jump lands on ground");
    player.reset();
    jump_course :: VMGEMap := vmge_map_parse("0,1,3,2,0;", 16);
    i = 0;
    while i < 80 {
        forward :: Float64 := 0.0;
        if i < 20 { forward = 1.0; }
        vmge_player_update(player, jump_course, forward, 0.0, false, false, i == 0, 0.02);
        i = i + 1;
    }
    vmge_check(player.grounded() && vmath.abs(player.camera.position().y - 3.8) < 0.001, "walking jump reaches a two-unit cube and lands on top");
    player.reset();
    low_roof :: VMGEMap := vmge_map_parse("0,3,0,2,0;", 16);
    vmge_player_update(player, low_roof, 0.0, 0.0, false, false, true, 0.05);
    vmge_check(player.camera.position().y <= 2.0 && player.state[1] == 0.0, "ascending jump stops at ceiling");
    player.reset();
    player.camera.set_position(vraylib.vec3(2.0, 5.0, 0.0));
    player.state[2] = 0.0;
    i = 0;
    while i < 80 { vmge_player_update(player, world, 0.0, 0.0, false, false, false, 0.02); i = i + 1; }
    vmge_check(player.grounded() && vmath.abs(player.camera.position().y - 3.8) < 0.001, "descending jump lands on cube top");
    player.camera.set_position(vraylib.vec3(-2.0, 3.8, 0.0));
    vmge_player_update(player, world, 0.0, 0.0, false, false, false, 0.02);
    vmge_check(!player.grounded(), "leaving support starts falling");
    vmge_check(vmge_snap(-2.9, 2.0) == -2.0 && vmge_snap(2.9, 2.0) == 2.0, "grid snapping on both sides of origin");
    # World allocated before frames: heavy transient allocations must not change
    # its backing store, even after repeated editing and capacity reuse.
    app.limit(100);
    while app.frame() {
        i = 0;
        while i < 20 {
            world.place(100.0, 1.0, 100.0, 2.0, 7);
            vmge_check(world.remove(100.0, 1.0, 100.0) == 1, "remove reused persistent row");
            temporary :: String := vmge_map_text(world);
            i = i + 1;
        }
        vmge_check(world.count() == 1 && world.value(0, 0) == 2.0, "map survives frame churn");
    }
} finally { app.close(); }
vmge_check(vraylib.live_handles() == 0, "headless collision cleanup");
out("VMGE_COLLISION_OK");
