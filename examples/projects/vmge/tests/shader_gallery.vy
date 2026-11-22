use "../src/shader/gallery.vy";
fn sgallery_check(ok :: Bool,message :: String) -> Bool { if !ok { throw message; } return true; }
fn sgallery_near(actual :: Float64,expected :: Float64,message :: String) -> Bool { return sgallery_check(vmath.abs(actual-expected) < 0.002,message+": "+string(actual)+" vs "+string(expected)); }
fn sgallery_uniform(shader :: vraylib.Shader,name :: String,gl :: Int64) -> Bool { rows :: Array := shader.uniforms(); i :: Int64 := 0; while i < rows.size() { row :: Array := rows[i]; if row[0] == name && int64(row[1]) == gl && int64(row[2]) == 1 && int64(row[3]) >= 0 { return true; } i = i+1; } return false; }
fn sgallery_pass(d :: SSDocument,source :: vraylib.RenderTarget,target :: vraylib.RenderTarget,defaults :: MaterialInstance) -> Bool {
    shader :: vraylib.Shader := ss_program(d); ss_material_apply_mode(d,defaults,true); bound :: Bool := false;
    try { shader.set_texture("inputTexture",source.texture()); bound = true; target.draw_from(source,shader); } finally { if bound { shader.set_texture("inputTexture",null); } } return true;
}
fn sgallery_plasma(m :: MaterialInstance,u :: Float64,v :: Float64,channel :: Int64) -> Float64 {
    time :: Float64 := ss_material_get(m,4,0); period :: Float64 := ss_material_get(m,5,0); phase :: Float64 := 6.28318530718*(time-vmath.floor(time/period)*period)/period;
    scale :: Float64 := ss_material_get(m,6,0); wave :: Float64 := 0.5+0.25*(vmath.sin(6.28318530718*(u-0.5)*scale+phase)+vmath.cos(6.28318530718*(v-0.5)*scale-phase)); weight :: Float64 := vmath.clamp((wave-0.5)*ss_material_get(m,7,0)+0.5,0.0,1.0);
    cells :: Float64 := ss_material_get(m,8,0); a :: Float64 := u*cells-0.5; b :: Float64 := v*cells-0.5; edge :: Float64 := vmath.min(vmath.abs(a-vmath.floor(a)-0.5),vmath.abs(b-vmath.floor(b)-0.5)); soft :: Float64 := vmath.clamp((edge-ss_material_get(m,9,0))/0.015,0.0,1.0); blend :: Float64 := (1.0-soft*soft*(3.0-2.0*soft))*ss_material_get(m,10,0);
    color :: Float64 := ss_material_get(m,1,channel)*(1.0-weight)+ss_material_get(m,2,channel)*weight; return (color*(1.0-blend)+ss_material_get(m,3,channel)*blend)*ss_material_get(m,11,0);
}
fn sgallery_lit(m :: MaterialInstance,point :: vraylib.Vector3,channel :: Int64) -> Float64 {
    lx :: Float64 := ss_material_get(m,3,0); ly :: Float64 := ss_material_get(m,3,1); lz :: Float64 := ss_material_get(m,3,2); ndl :: Float64 := vmath.max(lz/vmath.sqrt(lx*lx+ly*ly+lz*lz),0.0);
    dx :: Float64 := ss_material_get(m,11,0)-point.x; dy :: Float64 := ss_material_get(m,11,1)-point.y; dz :: Float64 := ss_material_get(m,11,2)-point.z; ndv :: Float64 := vmath.clamp(dz/vmath.sqrt(dx*dx+dy*dy+dz*dz),0.0,1.0);
    stripe :: Float64 := 0.5+0.5*vmath.sin(6.28318530718*ss_material_get(m,7,0)*(point.x+0.5*point.y+0.25*point.z)); blend :: Float64 := stripe*ss_material_get(m,8,0); albedo :: Float64 := ss_material_get(m,1,channel)*(1.0-blend)+ss_material_get(m,2,channel)*blend;
    ambient :: Float64 := ss_material_get(m,5,0); diffuse :: Float64 := ambient+(1.0-ambient)*vmath.pow(ndl,ss_material_get(m,6,0)); rim :: Float64 := ss_material_get(m,9,0)*vmath.pow(1.0-ndv,ss_material_get(m,10,0)); return (albedo*diffuse+rim)*ss_material_get(m,4,channel);
}
root :: String := "examples/projects/vmge/shaders/studio";
if vraylib.environment("VYNE_SHADER_GALLERY_AUTHOR") == "1" {
    @bounded<134217728> region gallery_author_fixture {
        gallery :: SSGallery := ss_gallery_sources(root); names :: Array := ss_gallery_names(); i :: Int64 := 0;
        while i < gallery.ids.size() { ss_document_save(gallery.documents[i],"build/shader-model-dev/gallery-"+names[i]+".vshader"); i = i+1; }
    };
    out("VMGE_SHADER_GALLERY_AUTHOR_OK");
} else {
    @bounded<134217728> region gallery_cpu {
        gallery :: SSGallery := ss_gallery_load(root); authored :: SSGallery := ss_gallery_sources(root); names :: Array := ss_gallery_names(); i :: Int64 := 0;
        while i < gallery.ids.size() {
            d :: SSDocument := gallery.documents[i]; a :: SSDocument := authored.documents[i]; sgallery_check(ss_document_text(d) == ss_document_text(a),"gallery companion source/schema bytes match shipped resource ID "+string(gallery.ids[i]));
            ss_document_save(d,"build/shader-model-dev/gallery-roundtrip-"+names[i]+".vshader"); reopened :: SSDocument := ss_document(); ss_document_load(reopened,"build/shader-model-dev/gallery-roundtrip-"+names[i]+".vshader"); sgallery_check(ss_document_text(reopened) == ss_document_text(d),"gallery UTF8 source/schema exact roundtrip"); i = i+1;
        }
        d :: SSDocument := ss_gallery_document(gallery,101); original :: Float64 := ss_parameter_get(d,1,2); build_revision :: Int64 := d.state[11];
        sgallery_check(ss_parameter_edit_metadata(d,1,"baseColor",[0.2,0.4,0.6],0.0,1.0,"Paint","linear RGB",true,false) && ss_parameter_get(d,1,2) == original && d.parameters.meta[0] == 1 && d.state[11] == build_revision,"metadata edit preserves stable ID/current components and source build revision");
        before :: String := ss_document_text(d); rejected :: Int64 := 0;
        try { ss_parameter_edit_metadata(d,1,"baseColor",[0.2,0.4,0.6],0.3,1.0,"Paint","linear RGB",true,false); } catch (error) { rejected = rejected+1; }
        try { ss_parameter_edit_metadata(d,1,"accentColor",[0.2,0.4,0.6],0.0,1.0,"Paint","linear RGB",true,false); } catch (error) { rejected = rejected+1; }
        try { ss_parameter_edit_metadata(d,1,"baseColor",[0.5],0.0,1.0,"Paint","linear RGB",true,false); } catch (error) { rejected = rejected+1; }
        try { ss_parameter_edit_metadata(d,1,"baseColor",[0.2,0.4,0.6],0.0,1.0,"Paint","linear RGB",false,true); } catch (error) { rejected = rejected+1; }
        sgallery_check(rejected == 4 && ss_document_text(d) == before,"metadata range/name/shape/refit rejection stages without mutation");
        builder :: Int64 := vcore.sb_create(); i = 0; while i < 130 { vcore.sb_append(builder,"x"); i = i+1; } failed :: Bool := false;
        try { ss_parameter_edit_metadata(d,1,"baseColor",[0.2,0.4,0.6],0.0,1.0,vcore.sb_build(builder),"linear RGB",true,false); } catch (error) { failed = true; } sgallery_check(failed && ss_document_text(d) == before,"metadata text capacity failure cannot publish partial name/default changes");
        sgallery_check(ss_parameter_edit_metadata(d,1,"paintColor",[0.2,0.4,0.6],0.0,1.0,"Paint","linear RGB",true,false) && d.state[11] == build_revision+1 && d.parameters.meta[0] == 1 && ss_parameter_get(d,1,2) == original,"uniform rename preserves stable values and requires a new build");
    };
    out("VMGE_SHADER_GALLERY_CPU_OK");
    if vraylib.environment("VYNE_SHADER_GPU") == "1" {
        app :: vraylib.App := vraylib.app_options(64,64,"VMGE starter gallery",0,vraylib.FLAG_WINDOW_HIDDEN);
        try {
            gallery :: SSGallery := ss_gallery_load(root); ss_gallery_compile(app,gallery); ss_gallery_prepare(app,gallery,16,16);
            i :: Int64 := 0; while i < gallery.ids.size() { d :: SSDocument := gallery.documents[i]; sgallery_check(ss_program(d).valid(),"gallery program valid"); j :: Int64 := 0; while j < d.parameters.state[0] { sgallery_check(d.parameters.meta[j*8+6] == 1,"every gallery editable parameter actively reflected for ID "+string(gallery.ids[i])); j = j+1; } i = i+1; }
            lit :: SSDocument := ss_gallery_document(gallery,101); plasma :: SSDocument := ss_gallery_document(gallery,102); tone :: SSDocument := ss_gallery_document(gallery,201); extract :: SSDocument := ss_gallery_document(gallery,202); horizontal :: SSDocument := ss_gallery_document(gallery,203); vertical :: SSDocument := ss_gallery_document(gallery,204);
            old_bank :: Int64 := gallery.pipeline.tokens[0]; ss_program(extract).set_texture("inputTexture",vraylib.RenderTarget(old_bank,app.token).texture()); blocked :: Bool := false; try { ss_gallery_prepare(app,gallery,32,32); } catch (error) { blocked = true; } sgallery_check(blocked && gallery.pipeline.tokens[0] == old_bank && ss_parameter_get(horizontal,1,0) == 0.0625 && ss_parameter_get(vertical,1,0) == 0.0625,"blocked gallery rebuild preserves old bank and both blur sampling pitches"); ss_program(extract).set_texture("inputTexture",null);
            sgallery_check(sgallery_uniform(ss_program(lit),"matNormal",35676) && sgallery_uniform(ss_program(lit),"matModel",35676) && sgallery_uniform(ss_program(lit),"mvp",35676),"custom lit vertex actively uses raylib model/normal/MVP mat4 semantics");
            lit_material :: MaterialInstance := ss_material_sized(101,1,16,0); ss_material_migrate(lit_material,lit); plasma_material :: MaterialInstance := ss_material_sized(102,2,16,0); ss_material_migrate(plasma_material,plasma); plasma_defaults :: MaterialInstance := ss_material_sized(102,3,16,0); ss_material_copy(plasma_defaults,plasma_material); defaults :: MaterialInstance := ss_material_sized(1,4,1,0);
            image :: vraylib.Image := app.image(1,1,vraylib.WHITE); texture :: vraylib.Texture := app.texture(image); image.close(); model :: vraylib.Model := app.cube(texture); camera :: vraylib.Camera := app.camera(vraylib.vec3(0.0,0.0,4.0),vraylib.vec3(0.0,0.0,0.0),60.0); camera.set_orthographic(4.0);
            lit_target :: vraylib.RenderTarget := app.float_target(64,64); dummy :: vraylib.RenderTarget := app.float_target(16,16); source :: vraylib.RenderTarget := app.float_target(16,16); output :: vraylib.RenderTarget := app.float_target(16,16); h_target :: vraylib.RenderTarget := app.float_target(16,16); v_target :: vraylib.RenderTarget := app.float_target(16,16);
            app.limit(2); while app.frame() {
                dummy.begin(); app.clear(vraylib.BLACK); dummy.end();
                lit_target.begin(); app.clear(vraylib.BLACK); camera.begin(); ss_material_set(lit_material,12,0,0.0); ss_material_draw(model,vraylib.math_matrix_identity(),lit,lit_material,0,vraylib.WHITE); camera.end(); lit_target.end();
                ray :: vraylib.Ray := camera.ray_viewport(vraylib.vec2(32.5,32.5),64,64); t :: Float64 := (0.5-ray.position.z)/ray.direction.z; point :: vraylib.Vector3 := vraylib.vec3(ray.position.x+t*ray.direction.x,ray.position.y+t*ray.direction.y,0.5); pixel :: Array<Float64> := lit_target.read_pixel(32,32); channel :: Int64 := 0;
                while channel < 3 { sgallery_near(pixel[channel],sgallery_lit(lit_material,point,channel),"stylized lit procedural pixel matches independent CPU reference"); channel = channel+1; }
                ss_program(lit).set_typed("matNormal",35676,ss_floats(16)); transform :: vraylib.Matrix := vraylib.math_matrix_multiply(vraylib.math_matrix_rotate_y(0.7853981633974483),vraylib.math_matrix_scale(1.6,1.0,0.6));
                lit_target.begin(); app.clear(vraylib.BLACK); camera.begin(); ss_material_set(lit_material,12,0,1.0); ss_material_draw(model,transform,lit,lit_material,0,vraylib.WHITE); camera.end(); lit_target.end(); normal :: Array<Float64> := lit_target.read_pixel(32,32); nx :: Float64 := 1.0/1.6; nz :: Float64 := 1.0/0.6; length :: Float64 := vmath.sqrt(nx*nx+nz*nz);
                sgallery_near(normal[0],0.5+0.5*nx/length,"matNormal overwrites poisoned matrix with inverse-transpose world normal x"); sgallery_near(normal[1],0.5,"normal debug y"); sgallery_near(normal[2],0.5+0.5*nz/length,"nonuniform normal transform is distinct from naive position transform");
                ss_material_copy(plasma_material,plasma_defaults); ss_material_apply(plasma,plasma_material); source.draw_from(dummy,ss_program(plasma)); pixel = source.read_pixel(8,8); channel = 0; while channel < 3 { sgallery_near(pixel[channel],sgallery_plasma(plasma_material,8.5/16.0,7.5/16.0,channel),"procedural plasma/grid matches CPU UV reference"); channel = channel+1; }
                original_pixels :: Array<Float64> := source.read_pixels(); ss_material_set(plasma_material,4,0,8.0); ss_material_apply(plasma,plasma_material); source.draw_from(dummy,ss_program(plasma)); repeated :: Array<Float64> := source.read_pixels(); i = 0; while i < original_pixels.size() { sgallery_check(original_pixels[i] == repeated[i],"plasma is exactly repeatable over its declared eight-second period"); i = i+1; }
                channel = 0; while channel < 3 { value :: Float64 := 2.0; if channel == 1 { value = 1.0; } if channel == 2 { value = 0.5; } ss_material_set(plasma_material,1,channel,value); ss_material_set(plasma_material,2,channel,value); channel = channel+1; } ss_material_set(plasma_material,10,0,0.0); ss_material_set(plasma_material,11,0,1.0); ss_material_apply(plasma,plasma_material); source.draw_from(dummy,ss_program(plasma)); hdr :: Array<Float64> := source.read_pixel(8,8); sgallery_near(hdr[0],2.0,"procedurally generated HDR fixture needs no external texture");
                sgallery_pass(tone,source,output,defaults); mapped :: Array<Float64> := output.read_pixel(8,8); channel = 0; while channel < 3 { sgallery_near(mapped[channel],1.0-vmath.exp(-hdr[channel]),"default linear exponential tonemap reference"); channel = channel+1; }
                ss_parameter_set(tone,1,0,1.0); sgallery_pass(tone,source,output,defaults); exposed :: Array<Float64> := output.read_pixel(8,8); sgallery_near(exposed[0],1.0-vmath.exp(-4.0),"one exposure stop doubles linear input"); ss_parameter_set(tone,1,0,0.0);
                sgallery_pass(extract,source,output,defaults); bright :: Array<Float64> := output.read_pixel(8,8); sgallery_near(bright[0],1.4,"bright extraction red threshold"); sgallery_near(bright[1],0.4,"bright extraction green threshold"); sgallery_near(bright[2],0.0,"below-threshold blue suppressed");
                result :: vraylib.RenderTarget := ss_pipeline_run(app,gallery.pipeline,gallery.documents,gallery.ids,source); combined :: Array<Float64> := vraylib.RenderTarget(gallery.pipeline.outputs[3],app.token).read_pixel(8,8); sgallery_near(combined[0],2.49,"bloom composite uses original input and shared blurred registry output"); sgallery_near(combined[1],1.126,"editable bloom tint applies before tonemap"); final_pixel :: Array<Float64> := result.read_pixel(8,8); sgallery_near(final_pixel[0],1.0-vmath.exp(-2.49),"complete five-pass bloom/tonemap reference");
                source.begin(); app.clear(vraylib.BLACK); vraylib.draw_rectangle(8,8,1,1,vraylib.WHITE); source.end(); sgallery_pass(horizontal,source,h_target,defaults); sgallery_pass(vertical,h_target,v_target,defaults); center_h :: Array<Float64> := h_target.read_pixel(8,8); center_v :: Array<Float64> := v_target.read_pixel(8,8); diagonal :: Array<Float64> := v_target.read_pixel(7,7);
                sgallery_near(center_h[0],0.375,"horizontal normalized five-tap kernel impulse"); sgallery_near(center_v[0],0.140625,"vertical separable kernel impulse"); sgallery_near(diagonal[0],0.0625,"blur actually spreads into diagonal neighbors"); result = ss_pipeline_run(app,gallery.pipeline,gallery.documents,gallery.ids,source); spread :: Array<Float64> := result.read_pixel(7,8); sgallery_near(spread[0],1.0-vmath.exp(-0.013125),"bloom spreads beyond original bright texel");
                source.begin(); app.clear(vraylib.rgba(255,128,64,128)); source.end(); alpha_source :: Array<Float64> := source.read_pixel(8,8); result = ss_pipeline_run(app,gallery.pipeline,gallery.documents,gallery.ids,source); alpha_result :: Array<Float64> := result.read_pixel(8,8); sgallery_check(alpha_result[3] == alpha_source[3],"all gallery passes preserve nonopaque alpha with replacement sampling");
                sgallery_check(app.passes() == 0 && app.can_release([source.token,h_target.token,v_target.token]),"standalone and pipeline sampler dependencies clear after use"); app.begin(); result.present(64.0,64.0); app.end();
            }
            model.close(); texture.close(); camera.close(); lit_target.close(); dummy.close(); source.close(); output.close(); h_target.close(); v_target.close(); ss_gallery_close(gallery); sgallery_check(app.resources() == 0,"gallery closes every real GPU program/target/model handle");
        } finally { app.close(); }
        out("VMGE_SHADER_GALLERY_GPU_OK");
    }
    out("VMGE_SHADER_GALLERY_OK");
}
