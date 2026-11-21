use "../src/editor/world_tools.vy";
use native vmem;
fn world_options_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
before :: Int64 := vmem.total_allocated();
@slab<Int64,256,512> region world_options_owner {
    names :: Array := []; paths :: Array := []; i :: Int64 := 0;
    while i < 384 { n :: Array<Int64> := slab_alloc(64); p :: Array<Int64> := slab_alloc(256); names.push(n); paths.push(p); i = i+1; }
    authored :: SEScene := se_scene(names,paths,0); runtime :: SEScene := se_scene(names,paths,128); staging :: SEScene := se_scene(names,paths,256);
    tools :: SEWorldTools := se_world_tools(); settings :: SEWorldSettings := tools.settings;
    se_world_settings_begin(settings,tools.numbers);
    se_world_settings_set(settings,0,"123456"); se_world_settings_set(settings,1,"48"); se_world_settings_set(settings,2,"12.75"); se_world_settings_set(settings,3,".75");
    se_world_settings_set(settings,4,".12"); se_world_settings_set(settings,5,"5"); se_world_settings_set(settings,6,".65"); se_world_settings_set(settings,7,"3");
    se_world_settings_set(settings,8,"4.25"); se_world_settings_set(settings,9,".33"); se_world_settings_set(settings,10,".5"); se_world_settings_set(settings,11,"-2.75");
    world_options_check(se_world_settings_apply(settings,tools.numbers) && tools.numbers[5] == 123456.0 && tools.numbers[6] == 48.0 && tools.numbers[3] == 0.33 && tools.numbers[1] == -2.75,"direct numeric fields publish together");
    se_world_settings_begin(settings,tools.numbers); se_world_settings_set(settings,0,"11"); se_world_settings_cancel(settings);
    world_options_check(tools.numbers[5] == 123456.0,"cancel keeps current options");
    se_world_settings_begin(settings,tools.numbers); se_world_settings_set(settings,0,"999"); se_world_settings_set(settings,1,"3.5");
    world_options_check(!se_world_settings_apply(settings,tools.numbers) && tools.numbers[5] == 123456.0 && settings.state[0] == 1,"invalid integer rejects entire settings transaction");
    failed :: Bool := false; try { se_world_settings_set(settings,4,"NaN"); } catch (error) { failed = true; }
    world_options_check(failed && settings.draft[21] == 0.12,"invalid text leaves numeric draft unchanged"); se_world_settings_cancel(settings);
    se_world_preview(tools,authored.world);
    world_options_check(authored.world.side() == 0 && tools.staging.side() == 48 && tools.staging.data[2] == 0.75,"preview uses independent terrain spacing and staged parameters");
    se_world_copy(authored.world,tools.staging); tools.state[5] = 0;
    se_terrain_generate_procedural(staging.world,48,0.75,12.75,123456,0,0.12,5,0.65,3.0);
    i = 0; while i < 4225 { world_options_check(authored.world.data[20496+i] == staging.world.data[20496+i],"fBm determinism"); i = i+1; }
    old :: Float64 := authored.world.data[20496+5*65+5];
    se_terrain_generate_procedural(staging.world,48,0.75,12.75,123456,0,0.24,5,0.65,3.0); world_options_check(staging.world.data[20496+5*65+5] != old,"frequency changes features");
    se_terrain_generate_procedural(staging.world,48,0.75,12.75,123456,0,0.12,1,0.65,3.0); world_options_check(staging.world.data[20496+5*65+5] != old,"octaves add detail");
    se_terrain_generate_procedural(staging.world,48,0.75,12.75,123456,0,0.12,5,0.0,3.0); world_options_check(staging.world.data[20496+5*65+5] != old,"persistence changes fine detail");
    world_options_check(se_world_spawn_point(authored.world,1.8) && se_terrain_normal_y(authored.world,authored.world.work[4235],authored.world.work[4237]) == 1.0,"central spawn pad has a flat rendered triangle");
    world_options_check(authored.world.work[4236] == se_terrain_height(authored.world,18.0,18.0)+1.8,"spawn eye uses exact terrain surface");
    text :: String := se_world_scene_text(authored); se_world_import_scene(runtime,staging,text);
    world_options_check(se_world_scene_text(runtime) == text && runtime.world.data[10] == 0.12 && runtime.world.data[11] == 5.0,"versioned generator settings and heights roundtrip");
    i = 0; while i < 24736 { world_options_check(runtime.world.data[i] == authored.world.data[i],"resource Float64 payload roundtrips exactly"); i = i+1; }
    failed = false; try { se_world_import_scene(authored,staging,text+"G|1|.1|4|.5|3|0|0\n"); } catch (error) { failed = true; }
    world_options_check(failed && se_world_scene_text(authored) == text,"duplicate generator import is atomic");
    failed = false; try { se_world_import_scene(authored,staging,se_scene_text(authored)+"G|1|0|4|.5|3|0|0\n"); } catch (error) { failed = true; }
    world_options_check(failed && se_world_scene_text(authored) == text,"invalid generator after real geometry staging is atomic");
    failed = false; try { se_generator_parse_record(authored.world,"G|2|.1|4|.5|3|0|0"); } catch (error) { failed = true; }
    world_options_check(failed && se_world_scene_text(authored) == text,"unsupported generator version rejected without mutation");
    disk_path :: String := "build/world-dev-procedural.vscene"; override := vraylib.environment("VYNE_VMGE_WORLD_OPTIONS_SCENE"); if override != null && override != "" { disk_path = override; }
    se_world_save_scene(authored,disk_path); se_world_load_scene(runtime,staging,disk_path);
    world_options_check(se_world_scene_text(runtime) == text,"procedural settings and height samples persist on disk");
    runtime.world.data[3] = 999999.0; se_terrain_generate_procedural(runtime.world,48,0.75,12.75,123456,0,0.12,5,0.65,3.0);
    se_world_parse_scene(staging,se_world_scene_text(runtime)); world_options_check(staging.world.data[14] == 1000017.0,"pad coordinates beyond center bound roundtrip with valid terrain origin");
    se_demo(staging); legacy :: String := se_scene_text(staging); se_world_import_scene(runtime,staging,legacy);
    world_options_check(runtime.count() == 5 && runtime.world.data[9] == 0.0,"extended reader retains v1 compatibility");
    se_clear(runtime); se_terrain_generate(runtime.world,1,1.0,0.0,42,3);
    runtime.world.data[20497] = 0.8; runtime.world.data[20561] = 0.0; runtime.world.data[20562] = 3.0;
    world_options_check(se_terrain_walkable(runtime.world,0.1,0.1) && !se_terrain_walkable(runtime.world,0.9,0.9),"50 degree limit uses each exact triangle normal");
    world_options_check(se_terrain_slope_blocked(runtime.world,0.7,0.8,0.9,0.9,2.0) && !se_terrain_slope_blocked(runtime.world,0.9,0.9,0.7,0.8,2.7),"steep ascent blocked; downhill escape allowed");
    matrices :: Array<Float64> := []; matrices.reserve(2048); matrices.resize(2048,0.0); cache :: SETransforms := se_transforms(matrices);
    runtime.world.data[20562] = 0.8;
    world_options_check(!se_world_motion_blocked(runtime,cache,vraylib.vec3(0.0,1.8,0.1),vraylib.vec3(0.25,1.8,0.1),1.8,-1,true) && vmath.abs(runtime.world.work[4238]-2.0) < 0.000001,"walkable 0.2 rise is not rejected by old .15 elevation proxy");
    obstacle :: Int64 := runtime.spawn(3,"Slope obstacle",-1); runtime.property(obstacle,0,0.25); runtime.property(obstacle,1,1.2); runtime.property(obstacle,2,0.1); se_update_transforms(runtime,cache);
    world_options_check(se_world_motion_blocked(runtime,cache,vraylib.vec3(0.0,1.8,0.1),vraylib.vec3(0.25,1.8,0.1),1.8,-1,true),"slope following retains node solid collisions");
    runtime.world.data[20497] = 2.0; runtime.world.data[20562] = 2.0;
    world_options_check(se_world_motion_blocked(runtime,cache,vraylib.vec3(0.0,1.8,0.1),vraylib.vec3(0.25,1.8,0.1),1.8,obstacle,true),"movement query rejects steep uphill triangle");
    app :: vraylib.App := vraylib.session(""); app.limit(96); warm :: Int64 := vmem.total_allocated(); committed :: Int64 := vmem.commit_total_allocated();
    while app.frame() {
        se_world_settings_begin(settings,tools.numbers); se_edit_begin(settings.editor,0,-1,string(app.frames()));
        world_options_check(se_world_settings_apply(settings,tools.numbers) && tools.numbers[5] == float64(app.frames()),"owned numeric editor across frames");
        se_world_settings_begin(settings,tools.numbers); se_edit_begin(settings.editor,9,-1,se_world_decimal_work(settings.numeric,tools.numbers[3]));
        world_options_check(se_world_settings_apply(settings,tools.numbers) && tools.numbers[3] == 0.33,"exact focused edit strings preserve fractional settings across frames");
        world_options_check(settings.editor.text.size() == 128 && settings.editor.edit_buffer.size() == 256 && settings.draft.size() == 32 && tools.numbers.size() == 32,"settings storage never grows");
    }
    app.close(); world_options_check(vmem.total_allocated()-warm <= 512 && vmem.commit_total_allocated() == committed,"direct settings edits retain bounded headers and zero commits");
    out("WORLD_OPTIONS_MEMORY headers=",vmem.total_allocated()-warm," commit_drift=",vmem.commit_total_allocated()-committed);
    se_clear(runtime); se_world_rectangle(runtime.world,0,0,0,63,63,1.0,0.5,3,false);
    se_terrain_generate_procedural(runtime.world,64,1.0,10.0,123456,1,0.1,8,0.8,3.0);
    @bounded<134217728> region maximum_extended_scene {
        maximum :: String := se_world_scene_text(runtime); se_world_parse_scene(staging,maximum);
        world_options_check(staging.world.count() == 4096 && staging.world.side() == 64 && staging.world.data[11] == 8.0,"maximum extended scene fits explicit allocation-volume bound");
    };
};
world_options_check(vmem.total_allocated() == before,"options owners reclaimed"); out("VMGE_WORLD_OPTIONS_OK");
