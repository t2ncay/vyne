use "../src/shader/graph.vy";
use "../src/shader/material.vy";
fn sg_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
fn sg_fixture(g :: SSGraph,reverse :: Bool) -> Bool {
    if reverse { ss_graph_add(g,30,7,4,"",0); ss_graph_add(g,20,3,4,"",0); ss_graph_add(g,11,1,1,"gain",1); ss_graph_add(g,10,0,4,"",0); } else { ss_graph_add(g,10,0,4,"",0); ss_graph_add(g,11,1,1,"gain",1); ss_graph_add(g,20,3,4,"",0); ss_graph_add(g,30,7,4,"",0); }
    ss_graph_value(g,10,0,0.8); ss_graph_value(g,10,1,0.4); ss_graph_value(g,10,2,0.2); ss_graph_value(g,10,3,2.0); ss_graph_value(g,11,0,0.5); ss_graph_connect(g,20,0,10); ss_graph_connect(g,20,1,11); ss_graph_connect(g,30,0,20); return true;
}
fn sg_texture(g :: SSGraph) -> Bool { ss_graph_add(g,1,1,5,"albedo",8); ss_graph_add(g,2,5,2,"",0); ss_graph_add(g,3,6,4,"",0); ss_graph_add(g,4,7,4,"",0); ss_graph_connect(g,3,0,1); ss_graph_connect(g,3,1,2); ss_graph_connect(g,4,0,3); return true; }
@bounded<67108864> region shader_graph_cpu {
    g :: SSGraph := ss_graph(); other :: SSGraph := ss_graph(); sg_fixture(g,false); sg_fixture(other,true); sg_check(ss_graph_glsl(g) == ss_graph_glsl(other),"GLSL topological order deterministic by stable ID");
    d :: SSDocument := ss_document(); sg_check(ss_graph_generate(g,d),"functional graph generation"); ss_graph_save(g,"build/shader-model-dev/roundtrip.vgraph"); ss_graph_load(other,"build/shader-model-dev/roundtrip.vgraph"); sg_check(ss_graph_glsl(g) == ss_graph_glsl(other),"typed graph save/reopen");
    sample :: SSGraph := ss_graph(); ss_graph_load(sample,"examples/projects/vmge/shaders/studio/color.vgraph"); sg_check(ss_graph_glsl(sample) == ss_graph_glsl(g),"shipped typed graph resource generates deterministic GLSL");
    ss_graph_connect(g,20,0,20); sg_check(!ss_graph_generate(g,d) && se_text_read(d.source) == ss_graph_glsl(other),"cycle fails before generated source publication"); ss_graph_connect(g,20,0,10);
    g.meta[ss_graph_slot(g,11)*8+2] = 2; failed :: Bool := false; try { ss_graph_validate(g); } catch (error) { failed = true; } sg_check(failed,"arithmetic type mismatch rejected");
    tex :: SSGraph := ss_graph(); sg_texture(tex); sg_check(ss_graph_generate(tex,d),"UV and texture sampling graph functional");
    mix_graph :: SSGraph := ss_graph(); ss_graph_add(mix_graph,1,0,4,"",0); ss_graph_add(mix_graph,2,0,4,"",0); ss_graph_add(mix_graph,3,0,1,"",0); ss_graph_add(mix_graph,4,4,4,"",0); ss_graph_add(mix_graph,5,7,4,"",0); ss_graph_connect(mix_graph,4,0,1); ss_graph_connect(mix_graph,4,1,2); ss_graph_connect(mix_graph,4,2,3); ss_graph_connect(mix_graph,5,0,4); sg_check(ss_graph_generate(mix_graph,d),"typed mix generation");
    bad_names :: Array := ["n_11","finalColor","fragTexCoord","uniform","vec4","texture","gl_bad","a__b","sampler2DRect","mvp"]; rejected :: Int64 := 0; name_index :: Int64 := 0; while name_index < bad_names.size() { bad :: SSGraph := ss_graph(); try { ss_graph_add(bad,11,1,1,bad_names[name_index],1); } catch (error) { rejected = rejected+1; } sg_check(bad.state[0] == 0,"reserved exposed name rejected before graph mutation"); name_index = name_index+1; } sg_check(rejected == bad_names.size(),"GLSL keyword/interface/generated-local exposed-name collisions rejected");
    mapped :: SSGraph := ss_graph(); sg_fixture(mapped,false); lines :: Array := ss_split(ss_graph_glsl(mapped),"\n"); mapped_declaration :: Bool := false; row :: Int64 := 1; while row < lines.size() { if lines[row] == "uniform float gain;" && lines[row-1] == "#line 1 11" { mapped_declaration = true; } row = row+1; } sg_check(mapped_declaration,"exposed declaration has its own node-specific source map"); se_text_write(mapped.names[ss_graph_slot(mapped,11)],"n_11"); failed = false; try { ss_graph_validate(mapped); } catch (error) { failed = true; } sg_check(failed,"validation catches direct exposed-name edits too");
};
out("VMGE_SHADER_GRAPH_CPU_OK");
if vraylib.environment("VYNE_SHADER_GPU") == "1" {
    app :: vraylib.App := vraylib.app_options(32,32,"Shader graph GPU",0,vraylib.FLAG_WINDOW_HIDDEN);
    try {
        g :: SSGraph := ss_graph(); sg_fixture(g,true); d :: SSDocument := ss_document(); sg_check(ss_graph_generate(g,d) && ss_compile(app,d),"generated graph actually compiles: "+se_text_read(d.log)); target :: vraylib.RenderTarget := app.float_target(8,8); m :: MaterialInstance := ss_material(1,1); ss_material_migrate(m,d);
        good_token :: Int64 := d.state[4]; good_source :: Int64 := ss_hash(d.source); se_text_write(g.names[ss_graph_slot(g,11)],"n_11"); sg_check(!ss_graph_generate(g,d) && d.state[4] == good_token && ss_program(d).valid() && ss_hash(d.source) == good_source,"exposed generated-local collision cannot replace live GPU graph source/program"); se_text_write(g.names[ss_graph_slot(g,11)],"gain");
        tex_graph :: SSGraph := ss_graph(); sg_texture(tex_graph); tex_doc :: SSDocument := ss_document(); sg_check(ss_graph_generate(tex_graph,tex_doc) && ss_compile(app,tex_doc),"generated texture graph actually compiles"); tex_material :: MaterialInstance := ss_material(2,1); ss_material_migrate(tex_material,tex_doc); image :: vraylib.Image := app.image(1,1,vraylib.GREEN); texture :: vraylib.Texture := app.texture(image); image.close(); ss_material_bind_texture(tex_material,8,42,texture);
        ss_material_save(tex_material,"build/shader-model-dev/sampler.vmaterial"); reopened_material :: MaterialInstance := ss_material(1,1); ss_material_load(reopened_material,"build/shader-model-dev/sampler.vmaterial"); sg_check(reopened_material.bindings[1] == 42 && reopened_material.bindings[2] == 0 && reopened_material.bindings[3] == 0,"sampler serialization stores resource IDs without native tokens");
        app.limit(2); while app.frame() {
            target.begin(); app.clear(vraylib.BLACK); ss_material_apply(d,m); ss_program(d).begin(); vraylib.draw_rectangle(0,0,8,8,vraylib.WHITE); ss_program(d).end(); target.end(); pixel :: Array<Float64> := target.read_pixel(4,4); sg_check(vmath.abs(pixel[0]-0.4) < 0.001 && vmath.abs(pixel[1]-0.2) < 0.001 && vmath.abs(pixel[2]-0.1) < 0.001,"typed graph generated pixels");
            target.begin(); app.clear(vraylib.BLACK); ss_material_apply(tex_doc,tex_material); ss_program(tex_doc).begin(); vraylib.draw_rectangle(0,0,8,8,vraylib.WHITE); ss_program(tex_doc).end(); target.end(); sampled :: Array<Float64> := target.read_pixel(4,4); sg_check(vmath.abs(sampled[1]-228.0/255.0) < 0.001 && vmath.abs(sampled[2]-48.0/255.0) < 0.001,"real typed UV texture graph sampling");
            app.begin(); target.present(32.0,32.0); app.end();
        }
        target.close(); ss_document_close(d); ss_document_close(tex_doc); texture.close(); sg_check(app.resources() == 0,"graph GPU releases handles");
    } finally { app.close(); }
    out("VMGE_SHADER_GRAPH_GPU_OK");
}
out("VMGE_SHADER_GRAPH_OK");
