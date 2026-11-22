// SPDX-License-Identifier: MIT
// Real GL integration plus deterministic bridge allocation failure injection.
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../../../vendor/raylib/src/raylib.h"
static void *allocations[4096];
static int live_allocations,remaining=-1;
static int reject(void) { if(remaining<0) return 0; if(!remaining) return 1; --remaining; return 0; }
static int allocation_slot(void *p) { for(int i=0;i<4096;++i) if(allocations[i]==p) return i; assert(0); return -1; }
static void *test_calloc(size_t n,size_t size) {
    if(reject()) return NULL; void *p=calloc(n,size);
    if(p) { allocations[allocation_slot(NULL)]=p; ++live_allocations; } return p;
}
static void *test_realloc(void *p,size_t size) {
    if(reject()) return NULL; int slot=allocation_slot(p); int fresh=!p;
    void *next=realloc(p,size); if(next) { allocations[slot]=next; live_allocations+=fresh; } return next;
}
static void test_free(void *p) { if(p) { allocations[allocation_slot(p)]=NULL; --live_allocations; } free(p); }
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
static void *test_proc(const char *name);
#define rlGetProcAddress test_proc
#include "../vraylib/bridge.c"
#undef rlGetProcAddress
extern void *rlGetProcAddress(const char *name);
#if defined(_WIN32)
#define TEST_ABI __stdcall
#else
#define TEST_ABI
#endif
static unsigned int gpu_shaders[1024],gpu_programs[1024];
static int shader_count,program_count,fail_create,fail_fullscreen;
static unsigned int fullscreen_buffer,fullscreen_vao;
static void (TEST_ABI *real_gen_buffers)(int,unsigned int *);
static void (TEST_ABI *real_gen_vaos)(int,unsigned int *);
static unsigned int (TEST_ABI *real_create_shader)(unsigned int);
static unsigned int (TEST_ABI *real_create_program)(void);
static unsigned char (TEST_ABI *is_shader)(unsigned int);
static unsigned char (TEST_ABI *is_program)(unsigned int);
static unsigned int TEST_ABI create_shader(unsigned int type) {
    if(fail_create==1) return 0;
    unsigned int id=real_create_shader(type); if(id) { assert(shader_count<1024); gpu_shaders[shader_count++]=id; } return id;
}
static unsigned int TEST_ABI create_program(void) {
    if(fail_create==2) return 0;
    unsigned int id=real_create_program(); if(id) { assert(program_count<1024); gpu_programs[program_count++]=id; } return id;
}
static void TEST_ABI gen_buffers(int n,unsigned int *ids) {
    if(fail_fullscreen==1) { for(int i=0;i<n;++i) ids[i]=0; return; }
    real_gen_buffers(n,ids); if(n) fullscreen_buffer=ids[0];
}
static void TEST_ABI gen_vaos(int n,unsigned int *ids) {
    if(fail_fullscreen==2) { for(int i=0;i<n;++i) ids[i]=0; return; }
    real_gen_vaos(n,ids); if(n) fullscreen_vao=ids[0];
}
static void *test_proc(const char *name) {
    void *p=rlGetProcAddress(name);
    if(!strcmp(name,"glCreateShader")) { memcpy(&real_create_shader,&p,sizeof(p)); unsigned int (TEST_ABI *fn)(unsigned int)=create_shader; memcpy(&p,&fn,sizeof(p)); }
    if(!strcmp(name,"glCreateProgram")) { memcpy(&real_create_program,&p,sizeof(p)); unsigned int (TEST_ABI *fn)(void)=create_program; memcpy(&p,&fn,sizeof(p)); }
    if(!strcmp(name,"glGenBuffers")) { memcpy(&real_gen_buffers,&p,sizeof(p)); void (TEST_ABI *fn)(int,unsigned int *)=gen_buffers; memcpy(&p,&fn,sizeof(p)); }
    if(!strcmp(name,"glGenVertexArrays")) { memcpy(&real_gen_vaos,&p,sizeof(p)); void (TEST_ABI *fn)(int,unsigned int *)=gen_vaos; memcpy(&p,&fn,sizeof(p)); }
    return p;
}
static void gpu_clean(unsigned int live_program) {
    void *p=rlGetProcAddress("glIsShader"); memcpy(&is_shader,&p,sizeof(p)); p=rlGetProcAddress("glIsProgram"); memcpy(&is_program,&p,sizeof(p));
    for(int i=0;i<shader_count;++i) assert(!is_shader(gpu_shaders[i]));
    for(int i=0;i<program_count;++i) if(gpu_programs[i]!=live_program) assert(!is_program(gpu_programs[i]));
    shader_count=0; program_count=0;
}

static VR text(const char *s) { return (VR){.kind=VR_TEXT,.text=s}; }
static VR list(VR *items,size_t count) { return (VR){.kind=VR_LIST,.items=items,.count=count}; }
static VR call(int id,VR *args,size_t count) {
    VyneRayResult r=vyne_ray_call(id,args,count); if(r.error) fprintf(stderr,"%s\n",r.error); assert(!r.error); return r.value;
}
static VyneRayResult config_result(int64_t session,const char *key,VR value) {
    VR args[]={vr_int(session),text(key),value}; return vyne_ray_call(-27,args,3);
}
static VR config(int64_t session,const char *key,VR *args,size_t n) {
    VyneRayResult r=config_result(session,key,list(args,n)); if(r.error) fprintf(stderr,"%s\n",r.error); assert(!r.error); return r.value;
}
static VR compile(int64_t session,const char *vertex,const char *fragment) {
    VR args[]={vertex ? text(vertex) : vr_null(),text(fragment)}; return config(session,"studio.compile",args,2);
}
static void close_resource(int64_t session,int64_t token) { VR a[]={vr_int(session),vr_int(token)}; call(-22,a,2); }
static void pass(int64_t session,int kind,int64_t token,int begin) { VR a[]={vr_int(session),vr_int(kind),vr_int(token),vr_bool(begin)}; call(-26,a,4); }
static void frame(int64_t session) { VR a=vr_int(session); call(-30,&a,1); }
static VR typed_result(int64_t session,int64_t token,const char *name,int type,double *values,size_t n,int fail) {
    VR numbers[64]; assert(n<=64); for(size_t i=0;i<n;++i) numbers[i]=vr_real(values[i]);
    VR args[]={vr_int(token),text(name),vr_int(type),list(numbers,n)};
    VyneRayResult r=config_result(session,"studio.typed",list(args,4));
    if(fail) { assert(r.error); return vr_null(); }
    if(r.error) fprintf(stderr,"%s\n",r.error); assert(!r.error && r.value.integer); return r.value;
}
static void pixel(int64_t session,int64_t token,int x,int y,double r,double g,double b) {
    VR args[]={vr_int(token),vr_int(x),vr_int(y)}; VR out=config(session,"studio.read_pixel",args,3);
    assert(out.count==4);
    if(fabs(out.items[0].real-r)>0.025 || fabs(out.items[1].real-g)>0.025 || fabs(out.items[2].real-b)>0.025)
        fprintf(stderr,"pixel %d,%d: %g %g %g expected %g %g %g\n",x,y,out.items[0].real,out.items[1].real,out.items[2].real,r,g,b);
    assert(fabs(out.items[0].real-r)<0.025 && fabs(out.items[1].real-g)<0.025 && fabs(out.items[2].real-b)<0.025);
    vyne_ray_dispose(out);
}
static int64_t float_target(int64_t session,int w,int h) { VR a[]={vr_int(w),vr_int(h)}; return config(session,"studio.float_target",a,2).integer; }
static int64_t adopt_model(int64_t session,Model model) {
    Model *copy=vr_alloc(sizeof(*copy)); *copy=model;
    int64_t token=vr_handle(copy,"Model",1,sizeof(*copy),vr_drop_UnloadModel,1,0).integer; vr_adopt(session,token); return token;
}
static const char *color_fragment="#version 330\nuniform vec4 color; out vec4 finalColor; void main(){finalColor=color;}";
static void test_builds(int64_t session,int64_t good) {
    unsigned int live=((Shader *)vr_checked(vr_int(good),"Shader",1))->id;
    const char *bad_vertex="#version 330\nvoid main(){this is invalid;}";
    const char *link_vertex="#version 330\nin vec3 vertexPosition;out vec3 mismatch;void main(){mismatch=vertexPosition;gl_Position=vec4(vertexPosition,1.0);}";
    const char *link_fragment="#version 330\nin vec2 mismatch;out vec4 finalColor;void main(){finalColor=vec4(mismatch,0,1);}";
    for(int i=0;i<3;++i) {
        VR out=compile(session,i==0 ? bad_vertex : i==2 ? link_vertex : NULL,i==2 ? link_fragment : i==1 ? "#version 330\nsyntax error" : color_fragment);
        assert(!out.items[1].integer && !out.items[0].integer && strlen(out.items[2].text)>0 && strlen(out.items[2].text)<VR_STUDIO_LOG);
        char saved[VR_STUDIO_LOG]; strcpy(saved,out.items[2].text);
        VR next=compile(session,NULL,color_fragment); assert(next.items[1].integer);
        assert(!strcmp(saved,out.items[2].text)); close_resource(session,next.items[0].integer);
        vyne_ray_dispose(next); vyne_ray_dispose(out); assert(vr_alive(good)); gpu_clean(live);
    }
    // Force candidate registration to exercise registry-growth failure too.
    while(1) {
        size_t used=0; for(size_t i=0;i<vr_capacity;++i) used+=vr_slots[i].data!=NULL;
        if(used==vr_capacity) break;
        VR buffer_args[]={text("int"),vr_int(1)}; int64_t token=call(VR_BUFFER,buffer_args,2).integer; vr_adopt(session,token);
    }
    int baseline=live_allocations;
    int successes=0;
    for(int fail=0;fail<24;++fail) {
        remaining=fail; VR a[]={vr_null(),text(color_fragment)}; VyneRayResult result=config_result(session,"studio.compile",list(a,2)); remaining=-1;
        if(!result.error) { assert(result.value.items[1].integer); close_resource(session,result.value.items[0].integer); vyne_ray_dispose(result.value); ++successes; }
        assert(!vr_temps && live_allocations==baseline && vr_alive(good)); gpu_clean(live);
    }
    assert(successes>0);
    for(int failure=1;failure<=2;++failure) {
        fail_create=failure; VR bad=compile(session,NULL,color_fragment); fail_create=0;
        assert(!bad.items[1].integer && !bad.items[0].integer && strstr(bad.items[2].text,"allocation")); vyne_ray_dispose(bad);
        assert(live_allocations==baseline); gpu_clean(live);
    }
}
static void test_reflection(int64_t session) {
    const char *source="#version 330\nuniform float samples[3];uniform ivec3 ints;uniform bvec2 flags;uniform mat4 matrix;uniform mat2 unsupported;uniform float inactive;uniform sampler2D source;out vec4 finalColor;void main(){finalColor=vec4(samples[0]+samples[1]+samples[2]+float(ints.x+ints.y+ints.z)+float(flags.x)+float(flags.y)+matrix[0][0]+unsupported[0][0])+texture(source,vec2(0.5));}";
    VR build=compile(session,NULL,source); assert(build.items[1].integer); int64_t token=build.items[0].integer;
    VR arg=vr_int(token); VR rows=config(session,"studio.uniforms",&arg,1); int found=0;
    for(size_t i=0;i<rows.count;++i) {
        VR row=rows.items[i]; assert(row.count==4 && row.items[3].integer>=0); assert(strcmp(row.items[0].text,"inactive"));
        if(!strcmp(row.items[0].text,"samples[0]")) { assert(row.items[1].integer==0x1406 && row.items[2].integer==3); ++found; }
        if(!strcmp(row.items[0].text,"ints")) { assert(row.items[1].integer==0x8B54); ++found; }
        if(!strcmp(row.items[0].text,"flags")) { assert(row.items[1].integer==0x8B57); ++found; }
        if(!strcmp(row.items[0].text,"matrix")) { assert(row.items[1].integer==0x8B5C); ++found; }
        if(!strcmp(row.items[0].text,"source")) { assert(row.items[1].integer==0x8B5E); ++found; }
    }
    assert(found==5); vyne_ray_dispose(rows);
    double floats[]={0.25,0.5,0.75}; typed_result(session,token,"samples",0x1406,floats,3,0); typed_result(session,token,"samples[0]",0x1406,floats,3,0);
    typed_result(session,token,"samples",0x1406,floats,2,1); typed_result(session,token,"samples",0x1404,floats,3,1);
    typed_result(session,token,"samples[1]",0x1406,floats,1,1); typed_result(session,token,"inactive",0x1406,floats,1,1);
    double ints[]={-2,3,4}; typed_result(session,token,"ints",0x8B54,ints,3,0);
    ints[0]=0.5; typed_result(session,token,"ints",0x8B54,ints,3,1); ints[0]=2147483648.0; typed_result(session,token,"ints",0x8B54,ints,3,1);
    double flags[]={0,1}; typed_result(session,token,"flags",0x8B57,flags,2,0); flags[0]=2; typed_result(session,token,"flags",0x8B57,flags,2,1);
    double matrix[16]; for(int i=0;i<16;++i) matrix[i]=i+1;
    typed_result(session,token,"matrix",0x8B5C,matrix,16,0);
    void (TEST_ABI *get_uniform)(unsigned int,int,float *); void *proc=rlGetProcAddress("glGetUniformfv"); memcpy(&get_uniform,&proc,sizeof(proc));
    Shader *shader=vr_checked(vr_int(token),"Shader",1); VRGL gl=vr_studio_gl(); float received[16]={0};
    get_uniform(shader->id,gl.GetUniformLocation(shader->id,"matrix"),received);
    for(int i=0;i<16;++i) assert(received[i]==(float)matrix[i]);
    typed_result(session,token,"unsupported",0x8B5A,matrix,4,1); typed_result(session,token,"source",0x8B5E,floats,1,1);
    floats[0]=NAN; typed_result(session,token,"samples",0x1406,floats,3,1); floats[0]=INFINITY; typed_result(session,token,"samples",0x1406,floats,3,1); floats[0]=1e300; typed_result(session,token,"samples",0x1406,floats,3,1);
    int baseline=live_allocations;
    for(int fail=0;fail<60;++fail) {
        remaining=fail; VyneRayResult result=config_result(session,"studio.uniforms",list(&arg,1)); remaining=-1;
        if(!result.error) vyne_ray_dispose(result.value); assert(!vr_temps && live_allocations==baseline);
    }
    close_resource(session,token); vyne_ray_dispose(build); gpu_clean(0);
    VR blocked=compile(session,NULL,"#version 330\nlayout(std140) uniform Params {vec4 blocked;};out vec4 finalColor;void main(){finalColor=blocked;}");
    assert(blocked.items[1].integer); arg=vr_int(blocked.items[0].integer); rows=config(session,"studio.uniforms",&arg,1); found=0;
    for(size_t i=0;i<rows.count;++i) if(strstr(rows.items[i].items[0].text,"blocked")) { assert(rows.items[i].items[3].integer==-1); ++found; }
    assert(found); double data[]={1,2,3,4}; typed_result(session,arg.integer,"blocked",0x8B52,data,4,1);
    vyne_ray_dispose(rows); close_resource(session,arg.integer); vyne_ray_dispose(blocked); gpu_clean(0);
    VR samplers=compile(session,NULL,"#version 330\nuniform sampler2D arraySampler[1];uniform samplerCube cubeSampler;out vec4 finalColor;void main(){finalColor=texture(arraySampler[0],vec2(0.5))+texture(cubeSampler,vec3(1,0,0));}");
    assert(samplers.items[1].integer); int64_t sampler_token=samplers.items[0].integer;
    VR target_args[]={vr_int(1),vr_int(1)}; int64_t target=config(session,"studio.float_target",target_args,2).integer;
    VR bind[]={vr_int(session),vr_int(sampler_token),text("arraySampler"),vr_int(target),vr_int(2)};
    assert(vyne_ray_call(-23,bind,5).error); bind[2]=text("cubeSampler"); assert(vyne_ray_call(-23,bind,5).error);
    close_resource(session,target); close_resource(session,sampler_token); vyne_ray_dispose(samplers); gpu_clean(0);
}
static void test_targets(int64_t session,int64_t shader) {
    int64_t target=float_target(session,8,4); VRTarget *logical=vr_checked(vr_int(target),"Target",1);
    int64_t view=target; // managed views keep the logical Target token
    frame(session); pass(session,1,target,1); ClearBackground(BLACK); pass(session,3,shader,1);
    double top[]={2.5,0.25,-0.5,1}; typed_result(session,shader,"color",0x8B52,top,4,0); DrawRectangle(0,0,8,2,WHITE);
    double bottom[]={0.1,3.75,0.5,1}; typed_result(session,shader,"color",0x8B52,bottom,4,0); DrawRectangle(0,2,8,2,WHITE);
    pass(session,3,shader,0); pass(session,1,target,0);
    pixel(session,target,0,0,2.5,0.25,-0.5); pixel(session,target,7,3,0.1,3.75,0.5);
    VRGL gl=vr_studio_gl(); gl.PixelStorei(0x0D05,8); gl.PixelStorei(0x0D02,17); gl.PixelStorei(0x0D03,3); gl.PixelStorei(0x0D04,2); gl.PixelStorei(0x0D00,1);
    pixel(session,target,0,0,2.5,0.25,-0.5); int state=0;
    gl.GetIntegerv(0x0D05,&state); assert(state==8); gl.GetIntegerv(0x0D02,&state); assert(state==17);
    gl.GetIntegerv(0x0D03,&state); assert(state==3); gl.GetIntegerv(0x0D04,&state); assert(state==2); gl.GetIntegerv(0x0D00,&state); assert(state==1);
    gl.PixelStorei(0x0D05,4); gl.PixelStorei(0x0D02,0); gl.PixelStorei(0x0D03,0); gl.PixelStorei(0x0D04,0); gl.PixelStorei(0x0D00,0);
    VR arg=vr_int(target); VR all=config(session,"studio.read_pixels",&arg,1); assert(all.count==128 && fabs(all.items[0].real-2.5)<0.01 && fabs(all.items[127-2].real-3.75)<0.01); vyne_ray_dispose(all);
    // Resize another target while a surface is bound: framebuffer is restored.
    int64_t other=float_target(session,2,2); pass(session,1,target,1); unsigned int framebuffer=rlGetActiveFramebuffer();
    VR resize[]={vr_int(session),vr_int(other),vr_int(3),vr_int(3)}; call(-32,resize,4); assert(rlGetActiveFramebuffer()==framebuffer); pass(session,1,target,0);
    resize[1]=vr_int(target); resize[2]=vr_int(16); resize[3]=vr_int(8); call(-32,resize,4);
    Texture texture=vr_managed_texture(session,view); assert(texture.width==16 && texture.height==8 && texture.format==PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);
    int allocation_baseline=live_allocations;
    for(int fail=0;fail<2;++fail) {
        VR resize_attempt[]={vr_int(session),vr_int(target),vr_int(9),vr_int(5)}; remaining=fail;
        VyneRayResult result=vyne_ray_call(-32,resize_attempt,4); remaining=-1;
        assert(result.error && live_allocations==allocation_baseline && !vr_temps);
        Texture preserved=vr_managed_texture(session,view); assert(preserved.id==texture.id && preserved.width==16 && preserved.height==8);
    }
    VR sampler_build=compile(session,NULL,"#version 330\nuniform sampler2D source;in vec2 fragTexCoord;out vec4 finalColor;void main(){finalColor=texture(source,fragTexCoord);}"); assert(sampler_build.items[1].integer);
    int64_t sampler=sampler_build.items[0].integer; VR binding[]={vr_int(session),vr_int(sampler),text("source"),vr_int(target),vr_int(2)}; call(-23,binding,5);
    pass(session,1,target,1); VR begin[]={vr_int(session),vr_int(3),vr_int(sampler),vr_bool(1)};
    VyneRayResult bad=vyne_ray_call(-26,begin,4); assert(bad.error && strstr(bad.error,"feedback") && vr_session(session)->depth==1);
    assert(vyne_ray_call(-23,binding,5).error); VR draw[]={vr_int(session),vr_int(target),vr_real(16),vr_real(8)}; assert(vyne_ray_call(-33,draw,4).error);
    pass(session,1,target,0);
    // A failed sampler begin leaves the good shader and target usable.
    // The logical dependency survives resize and prevents premature target release.
    VR release[]={vr_int(session),vr_int(target)}; assert(vyne_ray_call(-22,release,2).error);
    resize[2]=vr_int(8); resize[3]=vr_int(4); call(-32,resize,4);
    pass(session,1,target,1); ClearBackground((Color){64,128,192,255}); pass(session,1,target,0);
    pass(session,1,other,1); pass(session,3,sampler,1); DrawRectangle(0,0,3,3,WHITE); pass(session,3,sampler,0); pass(session,1,other,0);
    pixel(session,other,1,1,64.0/255,128.0/255,192.0/255);
    binding[3]=vr_int(0); call(-23,binding,5); close_resource(session,target); close_resource(session,other); close_resource(session,sampler); vyne_ray_dispose(sampler_build);
    (void)logical;
    // All target construction allocation failure points leave no handles/storage.
    int baseline=live_allocations; int successes=0;
    for(int fail=0;fail<16;++fail) {
        VR a[]={vr_int(2),vr_int(2)}; remaining=fail; VyneRayResult result=config_result(session,"studio.float_target",list(a,2)); remaining=-1;
        if(!result.error) { close_resource(session,result.value.integer); ++successes; }
        assert(!vr_temps && live_allocations==baseline);
    }
    assert(successes);
    // RGBA8 readback is normalized rather than returned as byte values.
    VR normal_args[]={vr_int(session),vr_int(2),vr_int(2),vr_bool(0)}; int64_t normal=call(-24,normal_args,4).integer;
    pass(session,1,normal,1); ClearBackground((Color){64,128,192,255}); pass(session,1,normal,0);
    pixel(session,normal,0,0,64.0/255,128.0/255,192.0/255);
    VR read_args[]={vr_int(normal),vr_int(-1),vr_int(0)}; assert(config_result(session,"studio.read_pixel",list(read_args,3)).error);
    baseline=live_allocations;
    for(int fail=0;fail<8;++fail) {
        VR arg=vr_int(normal); remaining=fail; VyneRayResult result=config_result(session,"studio.read_pixels",list(&arg,1)); remaining=-1;
        if(!result.error) vyne_ray_dispose(result.value); assert(live_allocations==baseline && !vr_temps);
    }
    close_resource(session,normal); gpu_clean(0);
}
static void test_materials(int64_t session,int64_t shader) {
    Model model={0}; model.transform=MatrixIdentity(); model.meshCount=2; model.materialCount=2;
    model.meshes=MemAlloc(2*sizeof(Mesh)); model.materials=MemAlloc(2*sizeof(Material)); model.meshMaterial=MemAlloc(2*sizeof(int));
    model.meshes[0]=GenMeshCube(0.7f,0.7f,0.7f); model.meshes[1]=GenMeshCube(0.7f,0.7f,0.7f);
    for(int i=0;i<model.meshes[1].vertexCount;++i) model.meshes[1].vertices[i*3]+=1.0f;
    UpdateMeshBuffer(model.meshes[1],0,model.meshes[1].vertices,model.meshes[1].vertexCount*3*(int)sizeof(float),0);
    model.materials[0]=LoadMaterialDefault(); model.materials[1]=LoadMaterialDefault(); model.meshMaterial[0]=0; model.meshMaterial[1]=1;
    int64_t token=adopt_model(session,model),target=float_target(session,100,100);
    Camera3D *camera=vr_alloc(sizeof(*camera)); *camera=(Camera3D){.position={0,0,5},.target={0,0,0},.up={0,1,0},.fovy=4,.projection=CAMERA_ORTHOGRAPHIC};
    int64_t cam=vr_handle(camera,"Camera3D",1,sizeof(*camera),NULL,1,0).integer; vr_adopt(session,cam);
    pass(session,1,target,1); ClearBackground(BLACK); pass(session,2,cam,1);
    Material original[2]; memcpy(original,model.materials,sizeof(original)); Matrix original_transform=model.transform;
    double red[]={1,0,0,1}; typed_result(session,shader,"color",0x8B52,red,4,0);
    VR transform=vr_pack_Matrix(MatrixTranslate(-0.75f,0.75f,0),0),tint=vr_pack_Color(WHITE,0);
    VR draw[]={vr_int(token),transform,vr_int(shader),vr_int(0),tint}; config(session,"studio.draw_material",draw,5);
    double green[]={0,1,0,1}; typed_result(session,shader,"color",0x8B52,green,4,0);
    vyne_ray_dispose(draw[1]); draw[1]=vr_pack_Matrix(MatrixTranslate(-0.75f,-0.75f,0),0); config(session,"studio.draw_material",draw,5);
    draw[3]=vr_int(2); assert(config_result(session,"studio.draw_material",list(draw,5)).error);
    assert(!memcmp(original,model.materials,sizeof(original)) && !memcmp(&original_transform,&model.transform,sizeof(Matrix)));
    pass(session,2,cam,0); pass(session,1,target,0);
    pixel(session,target,31,31,1,0,0); pixel(session,target,31,69,0,1,0); pixel(session,target,56,31,1,1,1);
    // Custom sampler uniforms on immediate meshes must bind independent instance
    // textures; batched SetShaderValueTexture alone does not bind DrawMesh units.
    VR sampled=compile(session,NULL,"#version 330\nuniform sampler2D source;out vec4 finalColor;void main(){finalColor=texture(source,vec2(0.5));}");
    assert(sampled.items[1].integer); int64_t sampler=sampled.items[0].integer;
    int64_t source_a=float_target(session,2,2),source_b=float_target(session,2,2);
    pass(session,1,source_a,1); ClearBackground(RED); pass(session,1,source_a,0);
    pass(session,1,source_b,1); ClearBackground(GREEN); pass(session,1,source_b,0);
    VR binding[]={vr_int(session),vr_int(sampler),text("source"),vr_int(source_a),vr_int(2)}; call(-23,binding,5);
    pass(session,1,target,1); ClearBackground(BLACK); pass(session,2,cam,1);
    vyne_ray_dispose(draw[1]); draw[1]=vr_pack_Matrix(MatrixTranslate(-0.75f,0.75f,0),0); draw[2]=vr_int(sampler); draw[3]=vr_int(0);
    config(session,"studio.draw_material",draw,5);
    binding[3]=vr_int(source_b); call(-23,binding,5);
    vyne_ray_dispose(draw[1]); draw[1]=vr_pack_Matrix(MatrixTranslate(-0.75f,-0.75f,0),0); config(session,"studio.draw_material",draw,5);
    pass(session,2,cam,0); pass(session,1,target,0);
    pixel(session,target,31,31,230.0/255,41.0/255,55.0/255); pixel(session,target,31,69,0,228.0/255,48.0/255);
    binding[3]=vr_int(target); call(-23,binding,5); pass(session,1,target,1); pass(session,2,cam,1);
    assert(config_result(session,"studio.draw_material",list(draw,5)).error);
    assert(!memcmp(original,model.materials,sizeof(original)) && !memcmp(&original_transform,&model.transform,sizeof(Matrix)));
    pass(session,2,cam,0); pass(session,1,target,0);
    binding[3]=vr_int(0); call(-23,binding,5); close_resource(session,sampler); vyne_ray_dispose(sampled); close_resource(session,source_a); close_resource(session,source_b);
    vyne_ray_dispose(draw[1]); vyne_ray_dispose(tint);
    // Permanent attachment preserves the original existing dependency semantics.
    VR attach[]={vr_int(token),vr_int(shader)}; call(VR_MODEL_SHADER,attach,2);
    VR release[]={vr_int(session),vr_int(shader)}; assert(vyne_ray_call(-22,release,2).error);
    close_resource(session,token); close_resource(session,target); close_resource(session,cam);
}
int main(void) {
    VR root=text(""); int64_t session=call(-15,&root,1).integer;
    VR window[]={vr_int(session),vr_int(100),vr_int(100),root,vr_int(0),vr_int(FLAG_WINDOW_HIDDEN)}; call(-25,window,6);
    SetTraceLogLevel(LOG_NONE);
    VR build=compile(session,NULL,color_fragment); assert(build.items[1].integer); int64_t good=build.items[0].integer;
    test_builds(session,good); test_reflection(session); test_targets(session,good); test_materials(session,good);
    close_resource(session,good); vyne_ray_dispose(build);
    for(int i=0;i<100;++i) { VR candidate=compile(session,NULL,color_fragment); assert(candidate.items[1].integer); close_resource(session,candidate.items[0].integer); vyne_ray_dispose(candidate); gpu_clean(0); }
    VR owner=vr_int(session); call(-18,&owner,1); assert(call(VR_LIVE,NULL,0).integer==0);
    call(VR_SHUTDOWN,NULL,0); assert(!vr_temps && live_allocations==1);
    // A new context must resolve fresh GL entry points and close its candidates.
    session=call(-15,&root,1).integer; window[0]=vr_int(session); call(-25,window,6);
    VR last=compile(session,NULL,color_fragment); assert(last.items[1].integer); vyne_ray_dispose(last);
    owner=vr_int(session); call(-18,&owner,1); assert(call(VR_LIVE,NULL,0).integer==0 && live_allocations==1);
    puts("SHADER_RUNTIME_NATIVE_OK");
}
