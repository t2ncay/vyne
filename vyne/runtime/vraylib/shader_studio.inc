// SPDX-License-Identifier: MIT
// Main-thread GL 3.3 studio operations. No GL headers, callbacks, or arena state.
#if defined(_WIN32)
#define VRGL_ABI __stdcall
#else
#define VRGL_ABI
#endif
#define VRGL_FUNCTIONS(X) \
 X(unsigned int,CreateShader,(unsigned int)) \
 X(void,ShaderSource,(unsigned int,int,const char *const *,const int *)) \
 X(void,CompileShader,(unsigned int)) \
 X(void,GetShaderiv,(unsigned int,unsigned int,int *)) \
 X(void,GetShaderInfoLog,(unsigned int,int,int *,char *)) \
 X(void,DeleteShader,(unsigned int)) \
 X(unsigned int,CreateProgram,(void)) \
 X(void,AttachShader,(unsigned int,unsigned int)) \
 X(void,DetachShader,(unsigned int,unsigned int)) \
 X(void,BindAttribLocation,(unsigned int,unsigned int,const char *)) \
 X(void,LinkProgram,(unsigned int)) \
 X(void,GetProgramiv,(unsigned int,unsigned int,int *)) \
 X(void,GetProgramInfoLog,(unsigned int,int,int *,char *)) \
 X(void,DeleteProgram,(unsigned int)) \
 X(int,GetUniformLocation,(unsigned int,const char *)) \
 X(int,GetAttribLocation,(unsigned int,const char *)) \
 X(void,GetActiveUniform,(unsigned int,unsigned int,int,int *,int *,unsigned int *,char *)) \
 X(void,GetIntegerv,(unsigned int,int *)) \
 X(void,UseProgram,(unsigned int)) \
 X(void,Uniform1fv,(int,int,const float *)) \
 X(void,Uniform2fv,(int,int,const float *)) \
 X(void,Uniform3fv,(int,int,const float *)) \
 X(void,Uniform4fv,(int,int,const float *)) \
 X(void,Uniform1iv,(int,int,const int *)) \
 X(void,Uniform2iv,(int,int,const int *)) \
 X(void,Uniform3iv,(int,int,const int *)) \
 X(void,Uniform4iv,(int,int,const int *)) \
 X(void,UniformMatrix4fv,(int,int,unsigned char,const float *)) \
 X(void,BindFramebuffer,(unsigned int,unsigned int)) \
 X(void,ReadPixels,(int,int,int,int,unsigned int,unsigned int,void *)) \
 X(void,PixelStorei,(unsigned int,int)) \
 X(void,BindBuffer,(unsigned int,unsigned int))
// Functions used for immediate model draws and explicit GL error reporting.
#define VRGL_EXTRA(X) \
 X(void,ActiveTexture,(unsigned int)) \
 X(void,BindTexture,(unsigned int,unsigned int)) \
 X(unsigned int,GetError,(void)) \
 X(void,BindRenderbuffer,(unsigned int,unsigned int)) \
 X(void,BlitFramebuffer,(int,int,int,int,int,int,int,int,unsigned int,unsigned int)) \
 X(void,ReadBuffer,(unsigned int)) \
 X(void,DrawBuffer,(unsigned int)) \
 X(unsigned char,IsEnabled,(unsigned int)) \
 X(void,Enable,(unsigned int)) \
 X(void,Disable,(unsigned int)) \
 X(void,GetBooleanv,(unsigned int,unsigned char *)) \
 X(void,ColorMask,(unsigned char,unsigned char,unsigned char,unsigned char)) \
 X(void,Viewport,(int,int,int,int)) \
 X(void,PolygonMode,(unsigned int,unsigned int)) \
 X(void,GenVertexArrays,(int,unsigned int *)) \
 X(void,DeleteVertexArrays,(int,const unsigned int *)) \
 X(void,BindVertexArray,(unsigned int)) \
 X(void,GenBuffers,(int,unsigned int *)) \
 X(void,DeleteBuffers,(int,const unsigned int *)) \
 X(void,BufferData,(unsigned int,ptrdiff_t,const void *,unsigned int)) \
 X(void,VertexAttribPointer,(unsigned int,int,unsigned int,unsigned char,int,const void *)) \
 X(void,EnableVertexAttribArray,(unsigned int)) \
 X(void,GetActiveAttrib,(unsigned int,unsigned int,int,int *,int *,unsigned int *,char *)) \
 X(void,DrawArrays,(unsigned int,int,int)) \
 X(void,GetUniformfv,(unsigned int,int,float *)) \
 X(void,GetUniformiv,(unsigned int,int,int *)) \
 X(void,BindSampler,(unsigned int,unsigned int))
typedef struct {
#define VRGL_FIELD(ret,name,args) ret (VRGL_ABI *name) args;
    VRGL_FUNCTIONS(VRGL_FIELD)
    VRGL_EXTRA(VRGL_FIELD)
#undef VRGL_FIELD
} VRGL;
static VRGL vr_studio_gl(void) {
    if(!IsWindowReady() || (rlGetVersion()!=RL_OPENGL_33 && rlGetVersion()!=RL_OPENGL_43)) vr_fail("shader studio requires a live desktop OpenGL 3.3 context");
    VRGL gl={0};
#define VRGL_LOAD(ret,name,args) { void *p=rlGetProcAddress("gl" #name); if(!p) vr_fail("required OpenGL entry point unavailable: gl" #name); memcpy(&gl.name,&p,sizeof(gl.name)); }
    VRGL_FUNCTIONS(VRGL_LOAD)
    VRGL_EXTRA(VRGL_LOAD)
#undef VRGL_LOAD
    return gl;
}
#undef VRGL_FUNCTIONS
#undef VRGL_EXTRA
#undef VRGL_ABI

// Returned trees are bridge-owned; temps make partially constructed trees safe.
static VR vr_studio_list(size_t n) {
    if(n>SIZE_MAX/sizeof(VR)) vr_fail("studio result size overflow");
    VR v={0}; v.kind=VR_LIST; v.count=n; v.items=vr_temp(n*sizeof(VR)); return v;
}
static VR vr_studio_text(const char *text) {
    VR v={0}; v.kind=VR_TEXT; size_t n=strlen(text)+1;
    char *p=vr_temp(n); memcpy(p,text,n); v.text=p; return v;
}
static VR vr_studio_return(VR v) {
    if(v.kind==VR_TEXT) vr_temp_keep((void *)v.text);
    if(v.kind==VR_LIST) { for(size_t i=0;i<v.count;++i) vr_studio_return(v.items[i]); vr_temp_keep(v.items); }
    return v;
}
static void vr_feedback(int64_t session,Texture texture) {
    VRSession *s=vr_session(session);
    for(int i=0;i<s->depth;++i) if(s->passes[i].kind==1) {
        VRTarget *t=vr_checked(vr_int(s->passes[i].handle),"Target",1);
        RenderTexture *rt=vr_checked(vr_int(t->texture),"RenderTexture",1);
        if(texture.id==rt->texture.id) vr_fail("feedback rejected: cannot sample the current render target");
    }
}
static void vr_studio_drop_shader(void *data,size_t count) {
    (void)count; Shader *shader=data;
    if(shader->id) rlUnloadShaderProgram(shader->id);
    free(shader->locs);
}
static const char *vr_studio_default_vertex=
    "#version 330\n"
    "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec4 vertexColor;\n"
    "out vec2 fragTexCoord; out vec4 fragColor; uniform mat4 mvp;\n"
    "void main(){fragTexCoord=vertexTexCoord;fragColor=vertexColor;gl_Position=mvp*vec4(vertexPosition,1.0);}\n";
enum { VR_STUDIO_LOG=16384, VR_STUDIO_UNIFORMS=4096, VR_STUDIO_NAME=4096 };
static void vr_studio_log(VRGL *gl,unsigned int object,int program,const char *stage,char *log) {
    int requested=0;
    if(program) gl->GetProgramiv(object,0x8B84,&requested); else gl->GetShaderiv(object,0x8B84,&requested);
    if(requested<=1) return;
    size_t used=strlen(log); if(used>=VR_STUDIO_LOG-64) return;
    int available=VR_STUDIO_LOG-(int)used;
    int prefix=snprintf(log+used,(size_t)available,"%s:\n",stage);
    if(prefix<0 || prefix>=available) return;
    used+=(size_t)prefix; int written=0;
    if(program) gl->GetProgramInfoLog(object,VR_STUDIO_LOG-(int)used-1,&written,log+used);
    else gl->GetShaderInfoLog(object,VR_STUDIO_LOG-(int)used-1,&written,log+used);
    log[VR_STUDIO_LOG-1]=0;
    if(requested>VR_STUDIO_LOG-(int)used-1) {
        const char *marker="\n[driver log truncated]\n";
        size_t marker_length=strlen(marker); memcpy(log+VR_STUDIO_LOG-marker_length-1,marker,marker_length+1); return;
    }
    used=strlen(log); if(used<VR_STUDIO_LOG-1) { log[used]='\n'; log[used+1]=0; }
}
static VR vr_studio_compile(int64_t session,VR args) {
    if(args.kind!=VR_LIST || args.count!=2) vr_fail("compile_shader requires vertex and fragment source");
    const char *vertex=vr_string(args.items[0]), *fragment=vr_required_string(args.items[1]);
    if(!vertex) vertex=vr_studio_default_vertex;
    VRGL gl=vr_studio_gl();
    VR out=vr_studio_list(3); out.items[0]=vr_int(0); out.items[1]=vr_bool(0);
    char *log=vr_temp(VR_STUDIO_LOG); out.items[2]=(VR){.kind=VR_TEXT,.text=log};
    Shader *shader=vr_temp(sizeof(*shader)); shader->locs=vr_temp(RL_MAX_SHADER_LOCATIONS*sizeof(int));
    for(int i=0;i<RL_MAX_SHADER_LOCATIONS;++i) shader->locs[i]=-1;
    int64_t token=vr_handle(shader,"Shader",1,sizeof(*shader),vr_studio_drop_shader,1,0).integer;
    vr_temp_keep(shader->locs); vr_temp_keep(shader); vr_adopt(session,token);
    unsigned int vs=gl.CreateShader(0x8B31), fs=gl.CreateShader(0x8B30), program=0;
    int vok=0,fok=0,linked=0;
    if(vs) { gl.ShaderSource(vs,1,&vertex,NULL); gl.CompileShader(vs); gl.GetShaderiv(vs,0x8B81,&vok); vr_studio_log(&gl,vs,0,"vertex",log); }
    if(fs) { gl.ShaderSource(fs,1,&fragment,NULL); gl.CompileShader(fs); gl.GetShaderiv(fs,0x8B81,&fok); vr_studio_log(&gl,fs,0,"fragment",log); }
    const char *attributes[]={"vertexPosition","vertexTexCoord","vertexNormal","vertexColor","vertexTangent","vertexTexCoord2","vertexBoneIndices","vertexBoneWeights"};
    if(vok && fok) {
        program=gl.CreateProgram();
        if(program) {
            gl.AttachShader(program,vs); gl.AttachShader(program,fs);
            for(unsigned int i=0;i<8;++i) gl.BindAttribLocation(program,i,attributes[i]);
            gl.LinkProgram(program); gl.GetProgramiv(program,0x8B82,&linked); vr_studio_log(&gl,program,1,"link",log);
        }
    }
    if(program) { gl.DetachShader(program,vs); gl.DetachShader(program,fs); }
    if(vs) gl.DeleteShader(vs); if(fs) gl.DeleteShader(fs);
    if(!linked) {
        if(program) gl.DeleteProgram(program);
        if(!vs || !fs || (vok && fok && !program)) snprintf(log,VR_STUDIO_LOG,"OpenGL shader/program allocation failed");
        else if(!*log) snprintf(log,VR_STUDIO_LOG,"%s failed without a driver diagnostic",!vok ? "vertex compilation" : !fok ? "fragment compilation" : "program link");
        vr_release(token,NULL); return vr_studio_return(out);
    }
    shader->id=program;
    int slots[]={SHADER_LOC_VERTEX_POSITION,SHADER_LOC_VERTEX_TEXCOORD01,SHADER_LOC_VERTEX_NORMAL,SHADER_LOC_VERTEX_COLOR,SHADER_LOC_VERTEX_TANGENT,SHADER_LOC_VERTEX_TEXCOORD02,SHADER_LOC_VERTEX_BONEIDS,SHADER_LOC_VERTEX_BONEWEIGHTS};
    for(int i=0;i<8;++i) shader->locs[slots[i]]=gl.GetAttribLocation(program,attributes[i]);
    shader->locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM]=gl.GetAttribLocation(program,"instanceTransform");
    const char *names[]={"mvp","matView","matProjection","matModel","matNormal","boneMatrices","colDiffuse","texture0","texture1","texture2"};
    int uniforms[]={SHADER_LOC_MATRIX_MVP,SHADER_LOC_MATRIX_VIEW,SHADER_LOC_MATRIX_PROJECTION,SHADER_LOC_MATRIX_MODEL,SHADER_LOC_MATRIX_NORMAL,SHADER_LOC_MATRIX_BONETRANSFORMS,SHADER_LOC_COLOR_DIFFUSE,SHADER_LOC_MAP_DIFFUSE,SHADER_LOC_MAP_SPECULAR,SHADER_LOC_MAP_NORMAL};
    for(int i=0;i<10;++i) shader->locs[uniforms[i]]=gl.GetUniformLocation(program,names[i]);
    out.items[0]=vr_int(token); out.items[1]=vr_bool(1); return vr_studio_return(out);
}
typedef struct { unsigned int type; int length,location,array; } VRReflection;
static int vr_studio_uniform_count(VRGL *gl,Shader shader) {
    int n=0,max=0; gl->GetProgramiv(shader.id,0x8B86,&n); gl->GetProgramiv(shader.id,0x8B87,&max);
    if(n<0 || n>VR_STUDIO_UNIFORMS || max>VR_STUDIO_NAME) vr_fail("shader reflection exceeds studio limits (4096 uniforms/name bytes)");
    return n;
}
static int vr_studio_find(VRGL *gl,Shader shader,const char *name,VRReflection *result) {
    int n=vr_studio_uniform_count(gl,shader);
    for(int i=0;i<n;++i) {
        char active[VR_STUDIO_NAME]={0}; int length=0,size=0; unsigned int type=0;
        gl->GetActiveUniform(shader.id,(unsigned int)i,sizeof(active),&length,&size,&type,active);
        active[sizeof(active)-1]=0;
        size_t len=strlen(active);
        int array=strchr(active,'[')!=NULL;
        // Whole-array uploads accept the reflected [0] spelling or base name.
        int match=!strcmp(name,active);
        if(len>=3 && !strcmp(active+len-3,"[0]")) { active[len-3]=0; match=match || !strcmp(name,active); }
        if(match) { *result=(VRReflection){type,size,gl->GetUniformLocation(shader.id,name),array}; return result->location>=0; }
    }
    return 0;
}
static void vr_studio_sampler(Shader shader,const char *name) {
    VRGL gl=vr_studio_gl(); VRReflection r={0};
    if(!vr_studio_find(&gl,shader,name,&r)) vr_fail("sampler is inactive or absent");
    if(r.type!=0x8B5E || r.length!=1 || r.array) vr_fail("set_texture supports only scalar sampler2D; sampler arrays/cube/integer/shadow samplers are unsupported");
}
static void vr_studio_unbind(VRSession *s,int64_t shader,VRUniform *u) {
    (void)s; u->sampler=0;
    VRDependency **edge=&vr_dependencies;
    while(*edge) { VRDependency *d=*edge; if(d->from==shader && d->key==1000+u->location) { *edge=d->next; free(d); } else edge=&d->next; }
}
static VR vr_studio_uniforms(Shader shader) {
    VRGL gl=vr_studio_gl(); int n=vr_studio_uniform_count(&gl,shader); VR out=vr_studio_list((size_t)n);
    for(int i=0;i<n;++i) {
        char name[VR_STUDIO_NAME]={0}; int length=0,size=0; unsigned int type=0;
        gl.GetActiveUniform(shader.id,(unsigned int)i,sizeof(name),&length,&size,&type,name); name[sizeof(name)-1]=0;
        VR row=vr_studio_list(4); row.items[0]=vr_studio_text(name); row.items[1]=vr_int(type);
        row.items[2]=vr_int(size); row.items[3]=vr_int(gl.GetUniformLocation(shader.id,name)); out.items[i]=row;
    }
    return vr_studio_return(out);
}
static VR vr_studio_typed(Shader shader,VR args) {
    const char *name=vr_required_string(args.items[1]); int type=vr_i32(args.items[2]); VR values=args.items[3];
    if(values.kind!=VR_LIST) vr_fail("set_typed requires an array of Float64 values");
    VRGL gl=vr_studio_gl(); VRReflection r={0};
    if(!vr_studio_find(&gl,shader,name,&r)) vr_fail("uniform is inactive, absent, a non-base array element, or in a uniform block");
    if(r.type!=(unsigned int)type) vr_fail("set_typed GL type does not match reflected uniform type");
    int components=0,integral=0;
    switch(type) {
    case 0x1406: components=1; break;
    case 0x8B50: components=2; break; case 0x8B51: components=3; break; case 0x8B52: components=4; break;
    case 0x1404: case 0x8B56: components=1; integral=1; break;
    case 0x8B53: case 0x8B57: components=2; integral=1; break;
    case 0x8B54: case 0x8B58: components=3; integral=1; break;
    case 0x8B55: case 0x8B59: components=4; integral=1; break;
    case 0x8B5C: components=16; break;
    default: vr_fail("unsupported uniform GL type: supported float/int/bool scalar/vector and mat4 only; samplers use set_texture");
    }
    if(r.length<1 || r.length>65536 || values.count!=(size_t)r.length*(size_t)components) vr_fail("set_typed value cardinality does not match reflected uniform array length/components");
    float *floats=vr_temp(values.count*sizeof(float)); int *ints=vr_temp(values.count*sizeof(int));
    int boolean=type>=0x8B56 && type<=0x8B59;
    for(size_t i=0;i<values.count;++i) {
        double value=vr_number(values.items[i]);
        if(!isfinite(value)) vr_fail("uniform values must be finite");
        if(integral) {
            if(value<INT_MIN || value>INT_MAX || trunc(value)!=value) vr_fail("integer uniform values must be exact signed 32-bit integers");
            if(boolean && value!=0.0 && value!=1.0) vr_fail("boolean uniform values must be 0 or 1");
            ints[i]=(int)value;
        } else { if(fabs(value)>FLT_MAX) vr_fail("uniform value exceeds finite Float32 range"); floats[i]=(float)value; }
    }
    // Flush queued draws before changing shared program state; restore GL binding.
    rlDrawRenderBatchActive(); int previous=0; gl.GetIntegerv(0x8B8D,&previous);
    while(gl.GetError()) {} gl.UseProgram(shader.id);
    if(type==0x8B5C) gl.UniformMatrix4fv(r.location,r.length,0,floats);
    else if(integral) {
        switch(components) { case 1: gl.Uniform1iv(r.location,r.length,ints); break; case 2: gl.Uniform2iv(r.location,r.length,ints); break; case 3: gl.Uniform3iv(r.location,r.length,ints); break; case 4: gl.Uniform4iv(r.location,r.length,ints); break; }
    } else {
        switch(components) { case 1: gl.Uniform1fv(r.location,r.length,floats); break; case 2: gl.Uniform2fv(r.location,r.length,floats); break; case 3: gl.Uniform3fv(r.location,r.length,floats); break; case 4: gl.Uniform4fv(r.location,r.length,floats); break; }
    }
    unsigned int error=gl.GetError(); gl.UseProgram((unsigned int)previous);
    if(error) vr_fail("OpenGL rejected the typed uniform upload");
    return vr_bool(1);
}
typedef struct {
    int read_fbo,draw_fbo,active,texture,renderbuffer,unpack_pbo;
    int alignment,row,skip_rows,skip_pixels,swap;
} VRAllocationState;
static VRAllocationState vr_allocation_save(VRGL *gl) {
    VRAllocationState s={0};
    gl->GetIntegerv(0x8CAA,&s.read_fbo); gl->GetIntegerv(0x8CA6,&s.draw_fbo);
    gl->GetIntegerv(0x84E0,&s.active); gl->GetIntegerv(0x8069,&s.texture); gl->GetIntegerv(0x8CA7,&s.renderbuffer);
    gl->GetIntegerv(0x88EF,&s.unpack_pbo); gl->GetIntegerv(0x0CF5,&s.alignment);
    gl->GetIntegerv(0x0CF2,&s.row); gl->GetIntegerv(0x0CF3,&s.skip_rows); gl->GetIntegerv(0x0CF4,&s.skip_pixels); gl->GetIntegerv(0x0CF0,&s.swap);
    return s;
}
static void vr_allocation_restore(VRGL *gl,VRAllocationState s) {
    gl->BindFramebuffer(0x8CA8,(unsigned int)s.read_fbo); gl->BindFramebuffer(0x8CA9,(unsigned int)s.draw_fbo);
    gl->ActiveTexture((unsigned int)s.active); gl->BindTexture(0x0DE1,(unsigned int)s.texture);
    gl->BindRenderbuffer(0x8D41,(unsigned int)s.renderbuffer); gl->BindBuffer(0x88EC,(unsigned int)s.unpack_pbo);
    gl->PixelStorei(0x0CF5,s.alignment); gl->PixelStorei(0x0CF2,s.row); gl->PixelStorei(0x0CF3,s.skip_rows);
    gl->PixelStorei(0x0CF4,s.skip_pixels); gl->PixelStorei(0x0CF0,s.swap);
}
static RenderTexture vr_studio_target(int width,int height,int format) {
    VRGL gl=vr_studio_gl(); VRAllocationState state=vr_allocation_save(&gl);
    // NULL must mean no upload, not byte offset zero in the caller's unpack PBO.
    gl.BindBuffer(0x88EC,0); gl.PixelStorei(0x0CF5,1); gl.PixelStorei(0x0CF2,0);
    gl.PixelStorei(0x0CF3,0); gl.PixelStorei(0x0CF4,0); gl.PixelStorei(0x0CF0,0);
    while(gl.GetError()) {}
    RenderTexture target=LoadRenderTextureEx(width,height,format);
    int complete=target.id && target.texture.id && target.depth.id && rlFramebufferComplete(target.id);
    unsigned int error=gl.GetError();
    if(!complete || error) { UnloadRenderTexture(target); target=(RenderTexture){0}; }
    vr_allocation_restore(&gl,state); return target;
}
static void vr_resize_target(int64_t session,int64_t h,int width,int height) {
    VRSession *s=vr_session(session); (void)vr_managed_resource(session,h);
    for(int i=0;i<s->depth;++i) if(s->passes[i].handle==h) vr_fail("cannot resize an active render target");
    if(width<1 || height<1 || width>16384 || height>16384) vr_fail("render target dimensions must be 1..16384");
    VRTarget *t=vr_checked(vr_int(h),"Target",1); RenderTexture old={0};
    if(t->texture) { old=*(RenderTexture *)vr_checked(vr_int(t->texture),"RenderTexture",1); if(old.texture.width==width && old.texture.height==height) return; }
    VRGL gl=vr_studio_gl(); // Resolve before reserving resources or changing state.
    RenderTexture *copy=vr_temp(sizeof(*copy)); VRTemp *guard=vr_temp_guard();
    int64_t next=vr_handle(copy,"RenderTexture",1,sizeof(*copy),vr_drop_UnloadRenderTexture,1,h).integer;
    vr_temp_keep(copy); guard->rollback=next;
    VRAllocationState state=vr_allocation_save(&gl);
    RenderTexture value=vr_studio_target(width,height,t->format ? t->format : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if(!IsRenderTextureValid(value)) vr_fail("render target allocation failed");
    // Rebind preflight before committing. Cleanup includes all GL state changed
    // by destroying the candidate (raylib's unload binds the candidate FBO).
    for(VRDependency *d=vr_dependencies;d;d=d->next)
        if(t->texture && vr_descendant(d->to,t->texture) && !vr_descendant(d->from,t->texture)) {
            UnloadRenderTexture(value); vr_allocation_restore(&gl,state);
            vr_fail("raw render attachment is still referenced; use the managed texture view");
        }
    *copy=value;
    if(t->texture) vr_release(t->texture,NULL);
    t->texture=next; guard->rollback=0;
    // If raw GL had the replaced storage bound, rebind its replacement instead
    // of trying to restore deleted GL names. Logical managed views keep tokens.
    if(old.id && state.read_fbo==(int)old.id) state.read_fbo=(int)value.id;
    if(old.id && state.draw_fbo==(int)old.id) state.draw_fbo=(int)value.id;
    if(old.texture.id && state.texture==(int)old.texture.id) state.texture=(int)value.texture.id;
    if(old.depth.id && state.renderbuffer==(int)old.depth.id) state.renderbuffer=(int)value.depth.id;
    vr_allocation_restore(&gl,state);
    for(VRModelTexture *b=s->models;b;b=b->next) if(b->texture==h) {
        Model *m=vr_checked(vr_int(b->model),"Model",1); SetMaterialTexture(&m->materials[b->material],MATERIAL_MAP_DIFFUSE,value.texture);
    }
}
static VR vr_studio_read(int64_t session,int64_t token,int single,int x,int y) {
    (void)vr_managed_resource(session,token); VRTarget *t=vr_checked(vr_int(token),"Target",1);
    RenderTexture *rt=vr_checked(vr_int(t->texture),"RenderTexture",1); VRGL gl=vr_studio_gl();
    int width=rt->texture.width,height=rt->texture.height;
    if(single && (x<0 || y<0 || x>=width || y>=height)) vr_fail("read_pixel coordinates are outside the target");
    size_t n=single ? 4 : (size_t)width*(size_t)height*4;
    // Bound memory and readback work (16M pixels, 64M components).
    if(n>67108864) vr_fail("read_pixels exceeds 16 million pixel readback limit; use read_pixel");
    float *pixels=vr_temp(n*sizeof(float)); VR out=vr_studio_list(n);
    rlDrawRenderBatchActive();
    int framebuffer=0,pack=0,pbo=0,row=0,skiprow=0,skippixel=0,swap=0;
    gl.GetIntegerv(0x8CAA,&framebuffer); gl.GetIntegerv(0x0D05,&pack); gl.GetIntegerv(0x88ED,&pbo);
    gl.GetIntegerv(0x0D02,&row); gl.GetIntegerv(0x0D03,&skiprow); gl.GetIntegerv(0x0D04,&skippixel);
    gl.GetIntegerv(0x0D00,&swap); gl.PixelStorei(0x0D00,0);
    gl.BindBuffer(0x88EB,0); gl.PixelStorei(0x0D05,1); gl.PixelStorei(0x0D02,0); gl.PixelStorei(0x0D03,0); gl.PixelStorei(0x0D04,0);
    gl.BindFramebuffer(0x8CA8,rt->id);
    while(gl.GetError()) {}
    gl.ReadPixels(single ? x : 0,single ? height-1-y : 0,single ? 1 : width,single ? 1 : height,0x1908,0x1406,pixels);
    unsigned int error=gl.GetError();
    gl.BindFramebuffer(0x8CA8,(unsigned int)framebuffer);
    gl.PixelStorei(0x0D05,pack); gl.PixelStorei(0x0D02,row); gl.PixelStorei(0x0D03,skiprow); gl.PixelStorei(0x0D04,skippixel); gl.BindBuffer(0x88EB,(unsigned int)pbo);
    gl.PixelStorei(0x0D00,swap);
    if(error) vr_fail("OpenGL render target readback failed");
    for(size_t i=0;i<n;++i) {
        size_t source=single ? i : ((size_t)(height-1)-(i/4)/(size_t)width)*(size_t)width*4+i%((size_t)width*4);
        out.items[i]=vr_real(pixels[source]);
    }
    return vr_studio_return(out);
}
static VR vr_studio_draw_material(int64_t session,VR args) {
    int64_t token=vr_integer(args.items[0]),shader_token=vr_integer(args.items[2]);
    (void)vr_managed_resource(session,token); (void)vr_managed_resource(session,shader_token);
    Model *model=vr_checked(vr_int(token),"Model",1); Shader *shader=vr_checked(vr_int(shader_token),"Shader",1);
    int slot=vr_i32(args.items[3]); if(slot<0 || slot>=model->materialCount) vr_fail("model material slot is out of bounds");
    Matrix world=vr_unpack_Matrix(args.items[1]); Color tint=vr_unpack_Color(args.items[4]);
    VRSession *s=vr_session(session); int camera=0;
    for(int i=0;i<s->depth;++i) if(s->passes[i].kind==2) camera=1;
    if(!s->frame || !s->opened || !camera) vr_fail("draw_material requires an active surface and camera");
    // Validate every mesh/material before any temporary mutation or GPU drawing.
    for(int i=0;i<model->meshCount;++i) if(model->meshMaterial[i]<0 || model->meshMaterial[i]>=model->materialCount) vr_fail("model contains an invalid mesh material slot");
    for(int i=0;i<model->materialCount;++i) {
        Material *m=&model->materials[i];
        for(int j=0;j<=MATERIAL_MAP_BRDF;++j) if(m->maps[j].texture.id) vr_feedback(session,m->maps[j].texture);
        if(i!=slot) for(VRUniform *u=s->uniforms;u;u=u->next)
            if(u->sampler && ((Shader *)vr_checked(vr_int(u->shader),"Shader",1))->id==m->shader.id) vr_feedback(session,vr_managed_texture(session,u->sampler));
    }
    vr_sampler_validate(session,shader_token);
    VRGL gl=vr_studio_gl();
    // Draw local material/map copies: shared Model memory never changes, even
    // transiently. Custom samplers use units 12..15, outside raylib map units.
    Matrix transform=MatrixMultiply(model->transform,world);
    rlDrawRenderBatchActive(); int previous_program=0,previous_active=0,bindings[4]={0};
    gl.GetIntegerv(0x8B8D,&previous_program); gl.GetIntegerv(0x84E0,&previous_active);
    for(int i=0;i<4;++i) { gl.ActiveTexture(0x84C0+12+(unsigned int)i); gl.GetIntegerv(0x8069,&bindings[i]); }
    for(int mesh=0;mesh<model->meshCount;++mesh) {
        int material=model->meshMaterial[mesh]; Material local=model->materials[material];
        MaterialMap maps[MATERIAL_MAP_BRDF+1]; memcpy(maps,local.maps,sizeof(maps)); local.maps=maps;
        if(material==slot) local.shader=*shader;
        Color color=maps[MATERIAL_MAP_DIFFUSE].color;
        maps[MATERIAL_MAP_DIFFUSE].color=(Color){(unsigned char)((unsigned int)color.r*tint.r/255),(unsigned char)((unsigned int)color.g*tint.g/255),(unsigned char)((unsigned int)color.b*tint.b/255),(unsigned char)((unsigned int)color.a*tint.a/255)};
        gl.UseProgram(local.shader.id); int unit=12;
        for(VRUniform *u=s->uniforms;u;u=u->next) {
            if(!u->sampler || ((Shader *)vr_checked(vr_int(u->shader),"Shader",1))->id!=local.shader.id) continue;
            Texture texture=vr_managed_texture(session,u->sampler); int map=-1;
            for(int i=0;i<3;++i) if(local.shader.locs[SHADER_LOC_MAP_DIFFUSE+i]==u->location) map=i;
            if(map>=0) maps[map].texture=texture;
            else {
                gl.ActiveTexture(0x84C0+(unsigned int)unit); gl.BindTexture(0x0DE1,texture.id);
                gl.Uniform1iv(u->location,1,&unit); ++unit;
            }
        }
        DrawMesh(model->meshes[mesh],local,transform);
    }
    for(int i=0;i<4;++i) { gl.ActiveTexture(0x84C0+12+(unsigned int)i); gl.BindTexture(0x0DE1,(unsigned int)bindings[i]); }
    gl.ActiveTexture((unsigned int)previous_active); gl.UseProgram((unsigned int)previous_program);
    return vr_bool(1);
}
static RenderTexture vr_studio_render_target(int64_t session,int64_t token) {
    (void)vr_managed_resource(session,token); VRTarget *target=vr_checked(vr_int(token),"Target",1);
    RenderTexture value=*(RenderTexture *)vr_checked(vr_int(target->texture),"RenderTexture",1);
    if(!IsRenderTextureValid(value)) vr_fail("render target has no valid storage");
    if(value.texture.format!=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 && value.texture.format!=PIXELFORMAT_UNCOMPRESSED_R32G32B32A32)
        vr_fail("replacement operations support RGBA8 and RGBA32F targets only");
    return value;
}
static void vr_studio_replacement_ready(int64_t session,RenderTexture destination,RenderTexture source,int frame) {
    VRSession *s=vr_session(session);
    if(s->depth) vr_fail("replacement operation requires no active render passes");
    if(frame && (!s->frame || !s->opened)) vr_fail("draw_from requires an active application frame");
    if(destination.id==source.id || destination.texture.id==source.texture.id) vr_fail("feedback rejected: replacement source and target must differ");
}
static VR vr_studio_copy(int64_t session,VR args) {
    RenderTexture destination=vr_studio_render_target(session,vr_integer(args.items[0]));
    RenderTexture source=vr_studio_render_target(session,vr_integer(args.items[1]));
    vr_studio_replacement_ready(session,destination,source,0);
    if(destination.texture.width!=source.texture.width || destination.texture.height!=source.texture.height)
        vr_fail("copy_from requires equal target dimensions; use draw_from for fullscreen scaling");
    if(source.texture.format!=destination.texture.format && source.texture.format!=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
        vr_fail("copy_from does not support narrowing RGBA32F to RGBA8; use draw_from for explicit shader conversion");
    VRGL gl=vr_studio_gl(); VRAllocationState state=vr_allocation_save(&gl);
    unsigned char scissor=gl.IsEnabled(0x0C11),srgb=gl.IsEnabled(0x8DB9);
    rlDrawRenderBatchActive();
    gl.BindFramebuffer(0x8CA8,source.id); gl.BindFramebuffer(0x8CA9,destination.id);
    int read_buffer=0,draw_buffer=0; gl.GetIntegerv(0x0C02,&read_buffer); gl.GetIntegerv(0x8825,&draw_buffer);
    gl.ReadBuffer(0x8CE0); gl.DrawBuffer(0x8CE0); gl.Disable(0x0C11); gl.Disable(0x8DB9);
    while(gl.GetError()) {}
    gl.BlitFramebuffer(0,0,source.texture.width,source.texture.height,0,0,destination.texture.width,destination.texture.height,0x4000,0x2600);
    unsigned int error=gl.GetError();
    gl.ReadBuffer((unsigned int)read_buffer); gl.DrawBuffer((unsigned int)draw_buffer);
    if(scissor) gl.Enable(0x0C11); if(srgb) gl.Enable(0x8DB9);
    vr_allocation_restore(&gl,state);
    if(error) vr_fail("OpenGL replacement framebuffer blit failed");
    return vr_bool(1);
}
static const unsigned int vr_fullscreen_caps[]={0x0BE2,0x0B71,0x0B90,0x0B44,0x0C11,0x8C89,0x8DB9,0x0BD0,0x809E,0x80A0};
typedef struct {
    VRAllocationState allocation;
    int viewport[4],program,vao,buffer,polygon[2],textures[5],samplers[5];
    unsigned char caps[sizeof(vr_fullscreen_caps)/sizeof(vr_fullscreen_caps[0])],mask[4];
} VRFullscreenState;
static VRFullscreenState vr_fullscreen_save(VRGL *gl) {
    VRFullscreenState state={0}; state.allocation=vr_allocation_save(gl);
    gl->GetIntegerv(0x0BA2,state.viewport); gl->GetIntegerv(0x8B8D,&state.program);
    gl->GetIntegerv(0x85B5,&state.vao); gl->GetIntegerv(0x8894,&state.buffer); gl->GetIntegerv(0x0B40,state.polygon);
    gl->GetBooleanv(0x0C23,state.mask);
    for(size_t i=0;i<sizeof(vr_fullscreen_caps)/sizeof(vr_fullscreen_caps[0]);++i) state.caps[i]=gl->IsEnabled(vr_fullscreen_caps[i]);
    for(int i=0;i<5;++i) { gl->ActiveTexture(0x84C0+(unsigned int)i); gl->GetIntegerv(0x8069,&state.textures[i]); gl->GetIntegerv(0x8919,&state.samplers[i]); }
    gl->ActiveTexture((unsigned int)state.allocation.active); return state;
}
static void vr_fullscreen_restore(VRGL *gl,VRFullscreenState state) {
    for(int i=0;i<5;++i) { gl->ActiveTexture(0x84C0+(unsigned int)i); gl->BindTexture(0x0DE1,(unsigned int)state.textures[i]); gl->BindSampler((unsigned int)i,(unsigned int)state.samplers[i]); }
    gl->BindVertexArray((unsigned int)state.vao); gl->BindBuffer(0x8892,(unsigned int)state.buffer); gl->UseProgram((unsigned int)state.program);
    gl->Viewport(state.viewport[0],state.viewport[1],state.viewport[2],state.viewport[3]); gl->PolygonMode(0x0408,(unsigned int)state.polygon[0]);
    gl->ColorMask(state.mask[0],state.mask[1],state.mask[2],state.mask[3]);
    for(size_t i=0;i<sizeof(vr_fullscreen_caps)/sizeof(vr_fullscreen_caps[0]);++i) {
        if(state.caps[i]) gl->Enable(vr_fullscreen_caps[i]); else gl->Disable(vr_fullscreen_caps[i]);
    }
    vr_allocation_restore(gl,state.allocation);
}
static int vr_fullscreen_uniform(VRGL *gl,Shader shader,const char *name,unsigned int type) {
    int location=gl->GetUniformLocation(shader.id,name); if(location<0) return -1;
    VRReflection r={0};
    if(!vr_studio_find(gl,shader,name,&r) || r.type!=type || r.length!=1 || r.array)
        vr_fail("fullscreen built-ins must be scalar texture0 sampler2D, mvp mat4, and colDiffuse vec4 when active");
    return location;
}
static VR vr_studio_fullscreen(int64_t session,VR args) {
    RenderTexture destination=vr_studio_render_target(session,vr_integer(args.items[0]));
    RenderTexture source=vr_studio_render_target(session,vr_integer(args.items[1]));
    vr_studio_replacement_ready(session,destination,source,1);
    int64_t shader_token=vr_integer(args.items[2]); (void)vr_managed_resource(session,shader_token);
    Shader shader=*(Shader *)vr_checked(vr_int(shader_token),"Shader",1); VRGL gl=vr_studio_gl();
    int mvp=vr_fullscreen_uniform(&gl,shader,"mvp",0x8B5C),diffuse=vr_fullscreen_uniform(&gl,shader,"colDiffuse",0x8B52);
    int primary=vr_fullscreen_uniform(&gl,shader,"texture0",0x8B5E);
    Texture textures[4]; int locations[4],sampler_values[4],count=0;
    for(VRUniform *u=vr_session(session)->uniforms;u;u=u->next) if(u->shader==shader_token && u->sampler) {
        Texture texture=vr_managed_texture(session,u->sampler);
        if(texture.id==destination.texture.id) vr_fail("feedback rejected: fullscreen sampler references destination");
        if(u->location==primary) continue; // texture0 is the explicit source parameter.
        if(count==4) vr_fail("fullscreen draw supports at most four additional managed samplers");
        textures[count]=texture; locations[count]=u->location; ++count;
    }
    int attributes=0,attribute_locations[3]={-1,-1,-1}; gl.GetProgramiv(shader.id,0x8B89,&attributes);
    if(attributes<0 || attributes>3) vr_fail("fullscreen vertex stage supports only vertexPosition, vertexTexCoord and vertexColor attributes");
    const char *names[]={"vertexPosition","vertexTexCoord","vertexColor"};
    for(int i=0;i<attributes;++i) {
        char name[VR_STUDIO_NAME]={0}; int length=0,size=0; unsigned int type=0;
        gl.GetActiveAttrib(shader.id,(unsigned int)i,sizeof(name),&length,&size,&type,name); int found=-1;
        for(int j=0;j<3;++j) if(!strcmp(name,names[j])) found=j;
        if(found<0 || size!=1 || (type!=0x1406 && (type<0x8B50 || type>0x8B52)))
            vr_fail("unsupported fullscreen vertex attribute: use scalar/float-vector vertexPosition, vertexTexCoord or vertexColor");
        attribute_locations[found]=gl.GetAttribLocation(shader.id,name);
    }
    float old_mvp[16]={0},old_diffuse[4]={0}; int old_primary=0;
    if(mvp>=0) gl.GetUniformfv(shader.id,mvp,old_mvp);
    if(diffuse>=0) gl.GetUniformfv(shader.id,diffuse,old_diffuse);
    if(primary>=0) gl.GetUniformiv(shader.id,primary,&old_primary);
    for(int i=0;i<count;++i) gl.GetUniformiv(shader.id,locations[i],&sampler_values[i]);
    VRFullscreenState state=vr_fullscreen_save(&gl); rlDrawRenderBatchActive();
    unsigned int vao=0,buffer=0; while(gl.GetError()) {}
    gl.GenVertexArrays(1,&vao); gl.GenBuffers(1,&buffer);
    gl.BindVertexArray(vao); gl.BindBuffer(0x8892,buffer);
    // Clip-space triangle and native bottom-left UVs preserve top-left logical
    // pixels without an extra vertical flip when writing another GL target.
    const float vertices[]={-1,-1,0, 0,0, 1,1,1,1, 3,-1,0, 2,0, 1,1,1,1, -1,3,0, 0,2, 1,1,1,1};
    gl.BufferData(0x8892,sizeof(vertices),vertices,0x88E4);
    int components[]={3,2,4},offsets[]={0,3,5};
    for(int i=0;i<3;++i) if(attribute_locations[i]>=0) {
        gl.VertexAttribPointer((unsigned int)attribute_locations[i],components[i],0x1406,0,9*sizeof(float),(const void *)(uintptr_t)(offsets[i]*sizeof(float)));
        gl.EnableVertexAttribArray((unsigned int)attribute_locations[i]);
    }
    gl.BindFramebuffer(0x8CA9,destination.id); int draw_buffer=0; gl.GetIntegerv(0x8825,&draw_buffer); gl.DrawBuffer(0x8CE0);
    gl.Viewport(0,0,destination.texture.width,destination.texture.height); gl.ColorMask(1,1,1,1); gl.PolygonMode(0x0408,0x1B02);
    for(size_t i=0;i<sizeof(vr_fullscreen_caps)/sizeof(vr_fullscreen_caps[0]);++i) gl.Disable(vr_fullscreen_caps[i]);
    gl.UseProgram(shader.id);
    const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},white[]={1,1,1,1};
    if(mvp>=0) gl.UniformMatrix4fv(mvp,1,0,identity); if(diffuse>=0) gl.Uniform4fv(diffuse,1,white);
    gl.ActiveTexture(0x84C0); gl.BindTexture(0x0DE1,source.texture.id); gl.BindSampler(0,0);
    if(primary>=0) { int unit=0; gl.Uniform1iv(primary,1,&unit); }
    for(int i=0;i<count;++i) {
        int unit=1+i; gl.ActiveTexture(0x84C0+(unsigned int)unit); gl.BindTexture(0x0DE1,textures[i].id); gl.BindSampler((unsigned int)unit,0);
        gl.Uniform1iv(locations[i],1,&unit);
    }
    unsigned int error=gl.GetError();
    if(vao && buffer && !error) { gl.DrawArrays(0x0004,0,3); error=gl.GetError(); }
    // No fallible bridge calls occur after state mutation; all cleanup precedes
    // diagnostics, including driver buffer/VAO allocation or draw failures.
    if(mvp>=0) gl.UniformMatrix4fv(mvp,1,0,old_mvp); if(diffuse>=0) gl.Uniform4fv(diffuse,1,old_diffuse);
    if(primary>=0) gl.Uniform1iv(primary,1,&old_primary);
    for(int i=0;i<count;++i) gl.Uniform1iv(locations[i],1,&sampler_values[i]);
    gl.DrawBuffer((unsigned int)draw_buffer);
    if(buffer) gl.DeleteBuffers(1,&buffer); if(vao) gl.DeleteVertexArrays(1,&vao);
    vr_fullscreen_restore(&gl,state);
    if(error || !vao || !buffer) vr_fail("OpenGL fullscreen replacement allocation/draw failed");
    return vr_bool(1);
}
static int vr_bank_contains(VR tokens,int64_t handle) {
    for(size_t i=0;i<tokens.count;++i) if(vr_descendant(handle,tokens.items[i].integer)) return 1;
    return 0;
}
static int vr_bank_preflight(int64_t session,VR tokens) {
    if(tokens.kind!=VR_LIST || tokens.count>65536) vr_fail("resource bank requires an array of at most 65536 Int64 tokens");
    if(vr_session(session)->depth) return 0;
    for(size_t i=0;i<tokens.count;++i) {
        VR v=tokens.items[i]; if((v.kind!=VR_INT && v.kind!=VR_HANDLE) || !vr_alive(v.integer) || v.integer==session) return 0;
        VRSlot *r=vr_slot(v.integer);
        // Banks contain managed ownership roots, never borrowed subresources.
        if(r->owner || r->session!=session || (!r->drop && !r->allocated)) return 0;
    }
    // Ownership descendants are in the bank too. Checking every incoming edge
    // covers chains/cycles: any externally rooted path crosses an incoming edge.
    for(VRDependency *d=vr_dependencies;d;d=d->next)
        if(vr_bank_contains(tokens,d->to) && !vr_bank_contains(tokens,d->from)) return 0;
    return 1;
}
static VR vr_bank_release(int64_t session,VR tokens) {
    if(!vr_bank_preflight(session,tokens)) vr_fail("resource bank release blocked: invalid/foreign/borrowed token, active pass, or external dependency");
    if(!tokens.count) return vr_bool(1);
    // Allocate/copy before any mutation. The neutral caller's array is untouched.
    int64_t *handles=vr_temp(tokens.count*sizeof(int64_t));
    for(size_t i=0;i<tokens.count;++i) handles[i]=tokens.items[i].integer;
    if(IsWindowReady()) rlDrawRenderBatchActive();
    VRDependency **edge=&vr_dependencies;
    while(*edge) {
        VRDependency *d=*edge;
        if(vr_bank_contains(tokens,d->from)) { *edge=d->next; free(d); } else edge=&d->next;
    }
    // No remaining incoming edges can block these roots. Duplicates are harmless.
    for(size_t i=0;i<tokens.count;++i) if(vr_alive(handles[i])) vr_release(handles[i],NULL);
    return vr_bool(1);
}
static VR vr_studio_config(int64_t session,const char *key,VR args) {
    if(!strcmp(key,"studio.can_release")) return vr_bool(vr_bank_preflight(session,args));
    if(!strcmp(key,"studio.release_bank")) return vr_bank_release(session,args);
    if(!strcmp(key,"studio.compile")) return vr_studio_compile(session,args);
    if(!strcmp(key,"studio.float_target")) {
        if(args.kind!=VR_LIST || args.count!=2) vr_fail("float_target requires width and height");
        (void)vr_studio_gl(); int width=vr_i32(args.items[0]),height=vr_i32(args.items[1]);
        if(width<1 || height<1 || width>16384 || height>16384) vr_fail("render target dimensions must be 1..16384");
        VRTarget *target=vr_temp(sizeof(*target)); target->format=PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
        VRTemp *guard=vr_temp_guard();
        int64_t token=vr_handle(target,"Target",1,sizeof(*target),NULL,1,0).integer;
        vr_temp_keep(target); guard->rollback=token; vr_adopt(session,token);
        // Register empty attachment before allocating GPU data.
        RenderTexture *copy=vr_temp(sizeof(*copy));
        int64_t attachment=vr_handle(copy,"RenderTexture",1,sizeof(*copy),vr_drop_UnloadRenderTexture,1,token).integer;
        vr_temp_keep(copy); target->texture=attachment;
        *copy=vr_studio_target(width,height,target->format);
        if(!IsRenderTextureValid(*copy)) vr_fail("RGBA32F render target allocation failed (no quantized fallback)");
        guard->rollback=0;
        return vr_int(token);
    }
    int arity=!strcmp(key,"studio.typed") ? 4 : !strcmp(key,"studio.draw_material") ? 5 : !strcmp(key,"studio.read_pixel") || !strcmp(key,"studio.draw_from") ? 3 : !strcmp(key,"studio.copy_from") ? 2 : 1;
    if(args.kind!=VR_LIST || args.count!=(size_t)arity) vr_fail("incorrect studio operation arguments");
    int64_t token=vr_integer(args.items[0]); (void)vr_managed_resource(session,token);
    if(!strcmp(key,"studio.copy_from")) return vr_studio_copy(session,args);
    if(!strcmp(key,"studio.draw_from")) return vr_studio_fullscreen(session,args);
    if(!strcmp(key,"studio.uniforms")) return vr_studio_uniforms(*(Shader *)vr_checked(vr_int(token),"Shader",1));
    if(!strcmp(key,"studio.typed")) return vr_studio_typed(*(Shader *)vr_checked(vr_int(token),"Shader",1),args);
    if(!strcmp(key,"studio.draw_material")) return vr_studio_draw_material(session,args);
    if(!strcmp(key,"studio.read_pixels")) return vr_studio_read(session,token,0,0,0);
    if(!strcmp(key,"studio.read_pixel")) return vr_studio_read(session,token,1,vr_i32(args.items[1]),vr_i32(args.items[2]));
    if(!strcmp(key,"studio.format")) return vr_int(vr_managed_texture(session,token).format);
    vr_fail("unknown shader studio operation"); return vr_null();
}
