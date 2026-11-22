// SPDX-License-Identifier: MIT
// Additional real-GL state, replacement-alpha, and release-bank transactions.
#include "../../../vendor/raylib/src/raylib.h"
static RenderTexture test_load_target(int width,int height,int format);
#define LoadRenderTextureEx test_load_target
#define main shader_previous_main
#include "shader_runtime.c"
#undef main
#undef LoadRenderTextureEx
static int fail_target,check_unpack;
static RenderTexture last_allocation;
static RenderTexture test_load_target(int width,int height,int format) {
    if(check_unpack) {
        VRGL gl=vr_studio_gl(); int n=0;
        gl.GetIntegerv(0x88EF,&n); assert(n==0); gl.GetIntegerv(0x0CF5,&n); assert(n==1);
        gl.GetIntegerv(0x0CF2,&n); assert(n==0); gl.GetIntegerv(0x0CF3,&n); assert(n==0);
        gl.GetIntegerv(0x0CF4,&n); assert(n==0); gl.GetIntegerv(0x0CF0,&n); assert(n==0);
    }
    RenderTexture target=LoadRenderTextureEx(width,height,format); last_allocation=target;
    if(fail_target) target.depth.id=0; // Real partial candidate, GPU cleanup required.
    return target;
}
static RenderTexture target_value(int64_t session,int64_t token) { return vr_studio_render_target(session,token); }
static void state_equal(VRGL *gl,VRAllocationState expected) {
    VRAllocationState actual=vr_allocation_save(gl); assert(!memcmp(&actual,&expected,sizeof(actual)));
}
static int is_object(const char *name,unsigned int id) {
    unsigned char (TEST_ABI *query)(unsigned int); void *p=rlGetProcAddress(name); memcpy(&query,&p,sizeof(p)); return query(id)!=0;
}
static int64_t buffer_token(int64_t session) {
    VR a[]={text("int"),vr_int(1)}; int64_t token=call(VR_BUFFER,a,2).integer; vr_adopt(session,token); return token;
}
static void test_allocation_state(int64_t session) {
    VRGL gl=vr_studio_gl(); VRAllocationState baseline=vr_allocation_save(&gl);
    int64_t first=float_target(session,2,2),second=float_target(session,2,2);
    RenderTexture a=target_value(session,first),b=target_value(session,second);
    unsigned int pbo=0; gl.GenBuffers(1,&pbo); gl.BindBuffer(0x88EC,pbo); unsigned char bytes[16]={0}; gl.BufferData(0x88EC,sizeof(bytes),bytes,0x88E4);
    gl.BindFramebuffer(0x8CA8,a.id); gl.BindFramebuffer(0x8CA9,b.id); gl.ActiveTexture(0x84C0+7);
    gl.BindTexture(0x0DE1,a.texture.id); gl.BindRenderbuffer(0x8D41,a.depth.id);
    gl.PixelStorei(0x0CF5,8); gl.PixelStorei(0x0CF2,19); gl.PixelStorei(0x0CF3,4); gl.PixelStorei(0x0CF4,3); gl.PixelStorei(0x0CF0,1);
    VRAllocationState perturbed=vr_allocation_save(&gl); check_unpack=1;
    int64_t created=float_target(session,3,2); state_equal(&gl,perturbed);
    // GPU partial-allocation failure cleans the candidate without losing caller state.
    int allocations_before=live_allocations; VR size[]={vr_int(3),vr_int(2)};
    fail_target=1; VyneRayResult failure=config_result(session,"studio.float_target",list(size,2)); fail_target=0;
    assert(failure.error && live_allocations==allocations_before && !vr_temps); state_equal(&gl,perturbed);
    assert(!is_object("glIsTexture",last_allocation.texture.id) && !is_object("glIsFramebuffer",last_allocation.id) && !is_object("glIsRenderbuffer",last_allocation.depth.id));
    for(int fail=0;fail<5;++fail) {
        remaining=fail; failure=config_result(session,"studio.float_target",list(size,2)); remaining=-1;
        assert(failure.error && live_allocations==allocations_before && !vr_temps); state_equal(&gl,perturbed);
    }
    VR resize[]={vr_int(session),vr_int(first),vr_int(3),vr_int(4)};
    fail_target=1; failure=vyne_ray_call(-32,resize,4); fail_target=0;
    assert(failure.error && target_value(session,first).id==a.id && live_allocations==allocations_before); state_equal(&gl,perturbed);
    assert(!is_object("glIsTexture",last_allocation.texture.id) && !is_object("glIsFramebuffer",last_allocation.id));
    for(int fail=0;fail<3;++fail) {
        remaining=fail; failure=vyne_ray_call(-32,resize,4); remaining=-1;
        assert(failure.error && target_value(session,first).id==a.id && live_allocations==allocations_before && !vr_temps); state_equal(&gl,perturbed);
    }
    // External raw attachment edge blocks commit after the new GPU candidate is built.
    int64_t borrowed=vr_target_texture(session,first),anchor=buffer_token(session); vr_depend(anchor,borrowed,123);
    allocations_before=live_allocations; failure=vyne_ray_call(-32,resize,4);
    assert(failure.error && target_value(session,first).id==a.id && live_allocations==allocations_before); state_equal(&gl,perturbed);
    assert(!is_object("glIsTexture",last_allocation.texture.id) && !is_object("glIsFramebuffer",last_allocation.id) && !is_object("glIsRenderbuffer",last_allocation.depth.id));
    close_resource(session,anchor); call(-32,resize,4);
    RenderTexture replacement=target_value(session,first); assert(replacement.id!=a.id && replacement.texture.width==3 && replacement.texture.height==4);
    perturbed.read_fbo=(int)replacement.id; perturbed.texture=(int)replacement.texture.id; perturbed.renderbuffer=(int)replacement.depth.id;
    state_equal(&gl,perturbed); assert(!vr_alive(borrowed));
    assert(!is_object("glIsTexture",a.texture.id) && !is_object("glIsFramebuffer",a.id) && !is_object("glIsRenderbuffer",a.depth.id));
    check_unpack=0; vr_allocation_restore(&gl,baseline); gl.DeleteBuffers(1,&pbo);
    close_resource(session,created); close_resource(session,first); close_resource(session,second); vr_allocation_restore(&gl,baseline);
}
static void clear_rows(int64_t session,int64_t token) {
    VRGL gl=vr_studio_gl(); VRAllocationState state=vr_allocation_save(&gl); RenderTexture target=target_value(session,token);
    void (TEST_ABI *clear_buffer)(unsigned int,int,const float *); void (TEST_ABI *scissor)(int,int,int,int);
    void *p=rlGetProcAddress("glClearBufferfv"); memcpy(&clear_buffer,&p,sizeof(p)); p=rlGetProcAddress("glScissor"); memcpy(&scissor,&p,sizeof(p));
    gl.BindFramebuffer(0x8CA9,target.id); gl.ColorMask(1,1,1,1); gl.Enable(0x0C11);
    const float top[]={2.5f,0.25f,-0.5f,0.25f},bottom[]={0.1f,3.75f,0.5f,0.6f};
    scissor(0,1,target.texture.width,1); clear_buffer(0x1800,0,top);
    scissor(0,0,target.texture.width,1); clear_buffer(0x1800,0,bottom);
    gl.Disable(0x0C11); vr_allocation_restore(&gl,state);
}
static VR pixels(int64_t session,int64_t token) { VR arg=vr_int(token); return config(session,"studio.read_pixels",&arg,1); }
static void pixels_compare(int64_t session,int64_t a,int64_t b,double tolerance) {
    VR left=pixels(session,a),right=pixels(session,b); assert(left.count==right.count);
    for(size_t i=0;i<left.count;++i) { if(fabs(left.items[i].real-right.items[i].real)>tolerance) fprintf(stderr,"RGBA mismatch [%zu]: %.9g %.9g\n",i,left.items[i].real,right.items[i].real); assert(fabs(left.items[i].real-right.items[i].real)<=tolerance); }
    vyne_ray_dispose(left); vyne_ray_dispose(right);
}
static void pixels_equal(int64_t session,int64_t a,int64_t b) { pixels_compare(session,a,b,0.0); }
static void fullscreen_state_equal(VRGL *gl,VRFullscreenState expected) {
    VRFullscreenState actual=vr_fullscreen_save(gl);
    size_t bytes=offsetof(VRFullscreenState,mask)+sizeof(actual.mask); // Exclude struct tail padding.
    if(memcmp(&actual,&expected,bytes)) {
        for(size_t i=0;i<bytes;++i) if(((unsigned char *)&actual)[i]!=((unsigned char *)&expected)[i]) fprintf(stderr,"state byte %zu: %u expected %u\n",i,((unsigned char *)&actual)[i],((unsigned char *)&expected)[i]);
    }
    assert(!memcmp(&actual,&expected,bytes));
}
static void test_replacement(int64_t session) {
    VRGL gl=vr_studio_gl(); VRFullscreenState baseline=vr_fullscreen_save(&gl);
    int64_t source=float_target(session,4,2),destination=float_target(session,4,2); clear_rows(session,source);
    VR source_pixels=pixels(session,source); assert(source_pixels.items[0].real==2.5 && source_pixels.items[3].real==0.25 && source_pixels.items[19].real==(double)0.6f); vyne_ray_dispose(source_pixels);
    gl.BindFramebuffer(0x8CA8,target_value(session,source).id); gl.BindFramebuffer(0x8CA9,target_value(session,destination).id);
    gl.Enable(0x0C11); gl.Enable(0x8DB9); gl.Enable(0x0BE2); gl.ColorMask(0,1,0,0);
    VRAllocationState state=vr_allocation_save(&gl); VR copy[]={vr_int(destination),vr_int(source)};
    config(session,"studio.copy_from",copy,2); state_equal(&gl,state); assert(gl.IsEnabled(0x0C11) && gl.IsEnabled(0x8DB9) && gl.IsEnabled(0x0BE2));
    pixels_equal(session,source,destination);
    copy[1]=vr_int(destination); assert(config_result(session,"studio.copy_from",list(copy,2)).error); copy[1]=vr_int(source);
    int64_t wrong_size=float_target(session,2,2); copy[0]=vr_int(wrong_size); assert(config_result(session,"studio.copy_from",list(copy,2)).error); copy[0]=vr_int(destination);
    VR normal_args[]={vr_int(session),vr_int(4),vr_int(2),vr_bool(0)}; int64_t normal=call(-24,normal_args,4).integer;
    copy[0]=vr_int(normal); assert(config_result(session,"studio.copy_from",list(copy,2)).error);
    vr_fullscreen_restore(&gl,baseline); clear_rows(session,normal); copy[0]=vr_int(destination); copy[1]=vr_int(normal); config(session,"studio.copy_from",copy,2); pixels_compare(session,normal,destination,1e-7);
    // Identity shader uses automatic texture0, no persistent binding needed.
    VR identity=compile(session,NULL,"#version 330\nin vec2 fragTexCoord;uniform sampler2D texture0;out vec4 finalColor;void main(){finalColor=texture(texture0,fragTexCoord);}"); assert(identity.items[1].integer);
    int64_t shader=identity.items[0].integer; VR draw[]={vr_int(destination),vr_int(source),vr_int(shader)};
    assert(config_result(session,"studio.draw_from",list(draw,3)).error); frame(session);
    double matrix[16]; for(int i=0;i<16;++i) matrix[i]=i+1; typed_result(session,shader,"mvp",0x8B5C,matrix,16,0);
    unsigned int vao=0,buffer=0; gl.GenVertexArrays(1,&vao); gl.GenBuffers(1,&buffer); gl.BindVertexArray(vao); gl.BindBuffer(0x8892,buffer);
    gl.BindFramebuffer(0x8CA8,target_value(session,source).id); gl.BindFramebuffer(0x8CA9,target_value(session,wrong_size).id);
    gl.ActiveTexture(0x84C0+7); gl.BindTexture(0x0DE1,target_value(session,normal).texture.id); gl.BindRenderbuffer(0x8D41,target_value(session,wrong_size).depth.id);
    gl.Viewport(3,5,7,11); gl.ColorMask(0,1,0,0); gl.PolygonMode(0x0408,0x1B01);
    void (TEST_ABI *blend_factors)(unsigned int,unsigned int,unsigned int,unsigned int);
    void (TEST_ABI *blend_equations)(unsigned int,unsigned int);
    void *proc=rlGetProcAddress("glBlendFuncSeparate"); memcpy(&blend_factors,&proc,sizeof(proc)); proc=rlGetProcAddress("glBlendEquationSeparate"); memcpy(&blend_equations,&proc,sizeof(proc));
    blend_factors(0x0302,0x0303,1,0); blend_equations(0x800B,0x800A);
    const unsigned int blend_enums[]={0x80C9,0x80C8,0x80CB,0x80CA,0x8009,0x883D}; int blend_saved[6];
    for(int i=0;i<6;++i) gl.GetIntegerv(blend_enums[i],&blend_saved[i]);
    for(size_t i=0;i<sizeof(vr_fullscreen_caps)/sizeof(vr_fullscreen_caps[0]);++i) gl.Enable(vr_fullscreen_caps[i]);
    gl.UseProgram(((Shader *)vr_checked(vr_int(shader),"Shader",1))->id);
    for(int blend=0;blend<=1;++blend) {
        if(blend) gl.Enable(0x0BE2); else gl.Disable(0x0BE2); VRFullscreenState expected=vr_fullscreen_save(&gl);
        config(session,"studio.draw_from",draw,3); fullscreen_state_equal(&gl,expected);
        for(int i=0;i<6;++i) { int actual=0; gl.GetIntegerv(blend_enums[i],&actual); assert(actual==blend_saved[i]); }
        // Readback does not depend on raster/blend/scissor state.
        pixels_equal(session,source,destination); vr_fullscreen_restore(&gl,expected);
        float restored[16]; gl.GetUniformfv(((Shader *)vr_checked(vr_int(shader),"Shader",1))->id,gl.GetUniformLocation(((Shader *)vr_checked(vr_int(shader),"Shader",1))->id,"mvp"),restored);
        for(int i=0;i<16;++i) assert(restored[i]==(float)matrix[i]);
        assert(!is_object("glIsBuffer",fullscreen_buffer) && !is_object("glIsVertexArray",fullscreen_vao));
        for(int fail=1;fail<=2;++fail) {
            fail_fullscreen=fail; VyneRayResult result=config_result(session,"studio.draw_from",list(draw,3)); fail_fullscreen=0;
            assert(result.error); fullscreen_state_equal(&gl,expected); pixels_equal(session,source,destination); vr_fullscreen_restore(&gl,expected);
            if(fail==1) assert(!is_object("glIsVertexArray",fullscreen_vao)); else assert(!is_object("glIsBuffer",fullscreen_buffer));
        }
    }
    vr_fullscreen_restore(&gl,baseline); blend_factors(0x0302,0x0303,0x0302,0x0303); blend_equations(0x8006,0x8006); gl.DeleteBuffers(1,&buffer); gl.DeleteVertexArrays(1,&vao);
    // Byte targets preserve nonopaque identity channels too, with no extra alpha
    // multiplication. Compare same-format stored bytes through float readback.
    int64_t byte_destination=call(-24,normal_args,4).integer; VR byte_copy[]={vr_int(byte_destination),vr_int(normal)};
    config(session,"studio.copy_from",byte_copy,2); pixels_equal(session,normal,byte_destination);
    VR byte_draw[]={vr_int(byte_destination),vr_int(normal),vr_int(shader)}; config(session,"studio.draw_from",byte_draw,3); pixels_equal(session,normal,byte_destination);
    pass(session,1,destination,1); assert(config_result(session,"studio.draw_from",list(draw,3)).error); assert(config_result(session,"studio.copy_from",list(copy,2)).error); pass(session,1,destination,0);
    draw[1]=vr_int(destination); assert(config_result(session,"studio.draw_from",list(draw,3)).error); draw[1]=vr_int(source);
    VR binding[]={vr_int(session),vr_int(shader),text("texture0"),vr_int(destination),vr_int(2)}; call(-23,binding,5); assert(config_result(session,"studio.draw_from",list(draw,3)).error); binding[3]=vr_int(0); call(-23,binding,5);
    VR unsupported=compile(session,"#version 330\nin vec3 vertexPosition;in vec3 vertexNormal;void main(){gl_Position=vec4(vertexPosition+vertexNormal,1);}",color_fragment); assert(unsupported.items[1].integer); draw[2]=unsupported.items[0]; assert(config_result(session,"studio.draw_from",list(draw,3)).error); close_resource(session,unsupported.items[0].integer); vyne_ray_dispose(unsupported);
    unsupported=compile(session,NULL,"#version 330\nuniform sampler2D texture0[1];in vec2 fragTexCoord;out vec4 finalColor;void main(){finalColor=texture(texture0[0],fragTexCoord);}"); assert(unsupported.items[1].integer); draw[2]=unsupported.items[0]; assert(config_result(session,"studio.draw_from",list(draw,3)).error); close_resource(session,unsupported.items[0].integer); vyne_ray_dispose(unsupported);
    close_resource(session,shader); vyne_ray_dispose(identity); close_resource(session,source); close_resource(session,destination); close_resource(session,wrong_size); close_resource(session,normal); close_resource(session,byte_destination); vr_fullscreen_restore(&gl,baseline);
}
static int can_release(int64_t session,VR *tokens,size_t count) { return config(session,"studio.can_release",tokens,count).integer!=0; }
static size_t edges(void) { size_t n=0; for(VRDependency *d=vr_dependencies;d;d=d->next) ++n; return n; }
static void test_banks(int64_t session) {
    int64_t a=buffer_token(session),b=buffer_token(session),outside=buffer_token(session),target=float_target(session,2,2);
    int64_t child=vr_target_texture(session,target); VR bank[]={vr_int(a),vr_int(b),vr_int(target),vr_int(a)};
    vr_depend(a,b,1); vr_depend(b,a,1); vr_depend(a,child,2); vr_depend(outside,b,3);
    assert(!can_release(session,bank,1) && !can_release(session,bank,4)); size_t before=edges(); int baseline=live_allocations;
    assert(config_result(session,"studio.release_bank",list(bank,4)).error); assert(edges()==before && live_allocations==baseline && vr_alive(a) && vr_alive(b) && vr_alive(target));
    close_resource(session,outside); assert(can_release(session,bank,4)); before=edges(); baseline=live_allocations;
    pass(session,0,0,1); assert(!can_release(session,bank,4)); assert(config_result(session,"studio.release_bank",list(bank,4)).error); pass(session,0,0,0);
    VR bad[]={vr_int(0)}; assert(!can_release(session,bad,1)); bad[0]=vr_int(session); assert(!can_release(session,bad,1)); bad[0]=vr_int(child); assert(!can_release(session,bad,1)); bad[0]=vr_int(outside); assert(!can_release(session,bad,1));
    VR buffer_args[]={text("int"),vr_int(1)}; int64_t foreign=call(VR_BUFFER,buffer_args,2).integer; bad[0]=vr_int(foreign); assert(!can_release(session,bad,1)); VR raw=vr_int(foreign); call(VR_RELEASE,&raw,1);
    assert(can_release(session,bank,4) && edges()==before && live_allocations==baseline);
    for(int fail=0;fail<2;++fail) {
        remaining=fail; VyneRayResult result=config_result(session,"studio.release_bank",list(bank,4)); remaining=-1;
        assert(result.error && edges()==before && live_allocations==baseline && vr_alive(a) && vr_alive(b) && vr_alive(target));
    }
    config(session,"studio.release_bank",bank,4); assert(!vr_alive(a) && !vr_alive(b) && !vr_alive(target) && !vr_alive(child));
    assert(bank[0].integer==a && bank[1].integer==b && bank[2].integer==target && !can_release(session,bank,4));
    assert(can_release(session,NULL,0)); config(session,"studio.release_bank",NULL,0);
}
int main(void) {
    VR root=text(""); int64_t session=call(-15,&root,1).integer;
    VR window[]={vr_int(session),vr_int(100),vr_int(100),root,vr_int(0),vr_int(FLAG_WINDOW_HIDDEN)}; call(-25,window,6); SetTraceLogLevel(LOG_NONE);
    test_allocation_state(session); test_replacement(session); test_banks(session); gpu_clean(0);
    VR owner=vr_int(session); call(-18,&owner,1); assert(call(VR_LIVE,NULL,0).integer==0 && !vr_temps && live_allocations==1);
    puts("SHADER_STATE_NATIVE_OK");
}
