use "../src/scene/history.vy";
use "../src/engine/world_resource_io.vy";
fn material_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
names :: Array := []; paths :: Array := []; i :: Int64 := 0;
while i < 384 { names.push(ss_text(63)); paths.push(ss_text(255)); i = i+1; }
scene :: SEScene := se_scene(names,paths,0); staging :: SEScene := se_scene(names,paths,128); play :: SEScene := se_scene(names,paths,256);
d :: SSDocument := ss_document(); se_demo(scene);
m :: MaterialInstance := scene.materials.instances[2]; ss_material_migrate(m,d); ss_material_set(m,1,0,0.375);
material_check(m.meta.size() == 80 && m.values.size() == 1024 && m.bindings.size() == 64,"scene material allocation uses 16 row / 16 sampler storage");
ss_parameter_define(d,2,"surfaceTexture",35678,1,0.0,0.0,0.0,"Surface","resource",false,false); ss_material_migrate(m,d);
resource :: Int64 := se_material_resource(scene.materials,"assets/surface.png");
m.binding_state[0] = 1; m.bindings[0] = 2; m.bindings[1] = resource; m.bindings[2] = 987654; m.bindings[3] = 999;
se_text_write(scene.materials.paths[2],"shaders/studio/gain.vshader"); m.state[0] = 3;
numeric :: Array := []; texts :: Array := []; i = 0; while i < 17 { numeric.push(ss_floats(2048)); texts.push(ss_ints(41988)); i = i+1; }
h :: SEHistory := se_history(numeric,texts); se_history_reset(h,scene,true);
snapshot :: SEMaterialSnapshot := h.materials[0];
material_check(snapshot.integers.size() == 19072 && snapshot.paths.size() == 32768 && snapshot.resources.size() == 16384,"history separates encoded sections to avoid integer-bank padding");
ss_material_set(m,1,0,0.75); se_record(h,scene); se_undo(h,scene);
material_check(ss_material_get(m,1,0) == 0.375,"undo snapshots material values"); se_redo(h,scene);
material_check(ss_material_get(m,1,0) == 0.75,"redo snapshots material values");
work :: Array<Int64> := ss_ints(128); copy :: Int64 := se_duplicate(scene,scene.handle(2),work);
other :: MaterialInstance := scene.materials.instances[copy%128]; ss_material_set(other,1,0,0.125);
material_check(ss_material_get(m,1,0) == 0.75 && other.state[1] != m.state[1],"duplicate has independent instance");
se_copy(play,scene); material_check(ss_material_get(play.materials.instances[2],1,0) == 0.75,"Play copy keeps overrides");
se_terrain_generate_procedural(scene.world,2,1.0,2.0,42,1,0.125,3,0.625,0.0);
se_world_parse_scene(staging,se_world_scene_text(scene));
material_check(ss_material_get(staging.materials.instances[2],1,0) == 0.75 && se_text_read(staging.materials.paths[2]) == "shaders/studio/gain.vshader","v3 resource roundtrip");
material_check(se_text_read(staging.materials.paths[copy%128]) != "" && ss_material_get(staging.materials.instances[copy%128],1,0) == 0.125,"second instance roundtrip");
reopened :: MaterialInstance := staging.materials.instances[2];
material_check(reopened.bindings[1] == resource && reopened.bindings[2] == 0 && reopened.bindings[3] == 0 && se_text_read(staging.materials.resources[0]) == "assets/surface.png","resource IDs/paths persist without GPU tokens");
material_check(staging.world.data[9] == 1.0 && staging.world.data[10] == 0.125 && staging.world.data[12] == 0.625,"v3 preserves world G record");
bad :: Bool := false; try { se_world_import_scene(scene,staging,"VMGE_SCENE|3\nM|2|3|3|00\n"); } catch (error) { bad = true; }
material_check(bad && scene.count() == 6 && ss_material_get(m,1,0) == 0.75,"invalid material import preserves authored state");
small :: MaterialInstance := ss_material_sized(1,1,1,0); full :: MaterialInstance := ss_material(1,1); ss_material_migrate(full,d);
bad = false; try { se_material_copy(small,full); } catch (error) { bad = true; }
material_check(bad && small.state[2] == 0,"capacity-aware copy rejects overflow before mutating destination");
se_material_copy(full,m); se_material_copy(full,full); material_check(ss_material_get(full,1,0) == 0.75,"material copy preserves values across capacity sizes and self-copy");
out("VMGE_MATERIAL_SCENE_OK");
