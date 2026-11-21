use "../src/engine/world_render.vy";
fn world_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
@bounded<16777216> region world_cpu {
    world :: SEWorld := se_world_create(); preview :: SEWorld := se_world_create();
    se_terrain_generate(world,8,2.0,6.0,42,0); se_terrain_generate(preview,8,2.0,6.0,42,0);
    i :: Int64 := 0; while i < 4225 { world_check(world.data[20496+i] == preview.data[20496+i],"seed determinism"); i = i+1; }
    se_terrain_generate(preview,8,2.0,6.0,43,0); world_check(world.data[20496] != preview.data[20496],"seed changes surface");
    se_terrain_generate(world,8,1.0,5.0,42,1); world_check(world.data[20496] == 0.0 && world.data[20496+4*65+4] > 0.0,"island coast and center");
    se_terrain_generate(world,8,1.0,5.0,42,2); world_check(world.data[20496+4*65+4] == 5.0 && world.data[20496] == 0.0,"plateau profile");
    se_terrain_generate(world,1,1.0,0.0,42,3);
    world.data[20496] = 0.0; world.data[20497] = 2.0; world.data[20561] = 4.0; world.data[20562] = 10.0;
    world_check(se_terrain_height(world,0.25,0.25) == 1.5,"first triangle interpolation");
    world_check(se_terrain_height(world,0.75,0.75) == 6.5,"second triangle differs from bilinear");
    world_check(se_terrain_height(world,1.0,1.0) == 10.0 && !se_terrain_contains(world,1.01,0.5),"bounded inclusive edge");
    ray :: vraylib.Ray := vraylib.Ray(vraylib.vec3(0.25,20.0,0.25),vraylib.vec3(0.0,-1.0,0.0));
    world_check(se_world_ray(world,ray) && vmath.abs(world.work[4227]-1.5) < 0.000001,"ray hits first rendered triangle");
    ray = vraylib.Ray(vraylib.vec3(0.75,20.0,0.75),vraylib.vec3(0.0,-1.0,0.0));
    world_check(se_world_ray(world,ray) && vmath.abs(world.work[4227]-6.5) < 0.000001 && world.work[4230] > 0.0,"ray hits second triangle with upward normal");
    se_world_paint(world,0.75,15.0,0.75,1.0,0);
    world_check(se_world_ray(world,ray) && world.work[4232] == 0.0 && world.work[4227] == 15.5 && world.work[4230] == 1.0,"map occludes terrain and supplies face normal");
    se_world_erase(world,0.75,15.0,0.75);
    se_terrain_generate(world,8,1.0,0.0,42,3);
    se_terrain_sculpt(world,4.0,4.0,2.0,3.0,0.0,0); world_check(world.data[20496+4*65+4] == 3.0 && world.data[20496+4*65+6] == 0.0,"radial raise falloff");
    se_terrain_sculpt(world,4.0,4.0,2.0,1.0,0.0,1); world_check(world.data[20496+4*65+4] == 2.0,"lower");
    se_terrain_sculpt(world,4.0,4.0,2.0,1.0,7.0,2); world_check(world.data[20496+4*65+4] == 7.0,"flatten target");
    se_world_copy(preview,world); se_terrain_sculpt(world,4.0,4.0,3.0,1.0,0.0,3);
    expected :: Float64 := (preview.data[20496+4*65+4]+preview.data[20496+4*65+3]+preview.data[20496+4*65+5]+preview.data[20496+3*65+4]+preview.data[20496+5*65+4])/5.0;
    world_check(world.data[20496+4*65+4] == expected,"smooth reads unmodified dab snapshot");
    se_world_rectangle(world,0,0,0,63,63,1.0,0.5,3,false); world_check(world.count() == 4096,"4096 cells without scene nodes");
    se_world_paint(world,0.0,0.5,0.0,1.0,4); world_check(world.count() == 4096 && world.data[20] == 4.0,"paint existing at capacity");
    failed :: Bool := false; try { se_world_rectangle(world,0,62,62,64,64,1.0,0.5,2,false); } catch (error) { failed = true; }
    world_check(failed && world.count() == 4096 && world.data[16+(62*64+62)*5+4] == 3.0,"rectangle capacity preflight atomic");
    se_world_rectangle(world,0,0,0,1,1,1.0,0.5,0,true); world_check(world.count() == 4092,"rectangle erase");
    se_world_rectangle(world,1,0,0,0,0,2.0,3.0,1,false); world_check(se_world_cell(world,0.0,0.0,3.0) >= 0,"XY plane");
    se_world_rectangle(world,2,0,0,0,0,2.0,5.0,1,false); world_check(se_world_cell(world,5.0,0.0,0.0) >= 0,"YZ plane");
    world_check(!se_world_paint(world,5.0,0.0,0.0,2.0,1),"no-op paint creates no edit");
};
out("VMGE_WORLD_OK");
