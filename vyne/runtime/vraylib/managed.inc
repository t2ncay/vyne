// SPDX-License-Identifier: MIT
// Persistent support for the Vyne external facade. This file contains no Vyne
// arena pointers: paths, caches, ownership and pass state survive frame rewinds.
typedef struct VRAsset { char *key; const char *kind; int64_t handle; struct VRAsset *next; } VRAsset;
typedef struct VRUniform {
    int64_t shader, sampler;
    char *name;
    int location;
    struct VRUniform *next;
} VRUniform;
typedef struct { int kind; int64_t handle; } VRPass;
typedef struct VRModelTexture { int64_t model,texture; int material; struct VRModelTexture *next; } VRModelTexture;
typedef struct {
    char *root;
    VRAsset *assets;
    VRUniform *uniforms;
    VRModelTexture *models;
    VRPass passes[32];
    int depth, frame, opened, audio, stopped;
    int64_t frames, limit, uniform_queries;
    double dt, elapsed;
    clock_t last_tick;
} VRSession;
typedef struct { int64_t texture; int follow; } VRTarget;

static char *vr_string_copy(const char *s) {
    size_t n=strlen(s)+1; char *p=vr_alloc(n); memcpy(p,s,n); return p;
}
static int vr_alive(int64_t h) {
    uint32_t i=(uint32_t)h;
    return i && i<=vr_capacity && vr_slots[i-1].data && vr_slots[i-1].generation==(uint32_t)((uint64_t)h>>32);
}
static VRSession *vr_session(int64_t h) { return vr_checked(vr_int(h),"Session",1); }
static int64_t vr_session_of(int64_t h) {
    VRSlot *r=vr_slot(h); while(r->owner) r=vr_slot(r->owner); return r->session;
}
static VRSlot *vr_managed_resource(int64_t session,int64_t h) {
    if(!vr_session(session) || vr_session_of(h)!=session) vr_fail("resource is closed or belongs to another application");
    return vr_slot(h);
}
static void vr_adopt(int64_t session,int64_t h) {
    (void)vr_session(session); VRSlot *r=vr_slot(h);
    if(r->owner || (r->session && r->session!=session)) vr_fail("only an owned resource can be adopted by an application");
    r->session=session;
    if(!strcmp(r->type,"Music") || !strcmp(r->type,"Sound")) vr_session(session)->audio=IsAudioDeviceReady();
}
static void vr_managed_invalidate(int64_t h) {
    if(!vr_managed_session || !vr_alive(vr_managed_session) || h==vr_managed_session) return;
    VRSession *s=vr_slots[(uint32_t)vr_managed_session-1].data;
    VRAsset **a=&s->assets;
    while(*a) {
        VRAsset *p=*a; if(p->handle==h) { *a=p->next; free(p->key); free(p); } else a=&p->next;
    }
    VRUniform **u=&s->uniforms;
    while(*u) {
        VRUniform *p=*u; if(p->shader==h) { *u=p->next; free(p->name); free(p); } else u=&p->next;
    }
    VRModelTexture **b=&s->models;
    while(*b) { VRModelTexture *p=*b; if(p->model==h) { *b=p->next; free(p); } else b=&p->next; }
}
static void vr_pass_end(VRSession *s) {
    VRPass p=s->passes[--s->depth];
    switch(p.kind) {
    case 0: EndDrawing(); break;
    case 1: EndTextureMode(); break;
    case 2: EndMode3D(); break;
    case 3: EndShaderMode(); break;
    case 4: EndScissorMode(); break;
    }
}
static void vr_finish(VRSession *s,int completed) {
    while(s->depth) vr_pass_end(s);
    if(s->frame && completed) ++s->frames;
    s->frame=0;
}
static Texture vr_managed_texture(int64_t session,int64_t h) {
    VRSlot *r=vr_managed_resource(session,h);
    if(!strcmp(r->type,"Texture")) return *(Texture *)r->data;
    if(!strcmp(r->type,"Target")) {
        VRTarget *t=r->data; RenderTexture *rt=vr_checked(vr_int(t->texture),"RenderTexture",1); return rt->texture;
    }
    vr_fail("expected a texture or managed render target"); return (Texture){0};
}
static int64_t vr_target_texture(int64_t session,int64_t h) {
    VRSlot *r=vr_managed_resource(session,h);
    if(!strcmp(r->type,"Texture")) return h;
    VRTarget *t=vr_checked(vr_int(h),"Target",1);
    RenderTexture *rt=vr_checked(vr_int(t->texture),"RenderTexture",1);
    return vr_handle(&rt->texture,"Texture",1,sizeof(Texture),NULL,0,t->texture).integer;
}
static void vr_resize_target(int64_t session,int64_t h,int width,int height) {
    VRSession *s=vr_session(session); (void)vr_managed_resource(session,h);
    for(int i=0;i<s->depth;++i) if(s->passes[i].handle==h) vr_fail("cannot resize an active render target");
    if(width<1 || height<1 || width>16384 || height>16384) vr_fail("render target dimensions must be 1..16384");
    VRTarget *t=vr_checked(vr_int(h),"Target",1);
    if(t->texture) { RenderTexture *old=vr_checked(vr_int(t->texture),"RenderTexture",1); if(old->texture.width==width && old->texture.height==height) return; }
    RenderTexture value=LoadRenderTexture(width,height);
    if(!IsRenderTextureValid(value)) { UnloadRenderTexture(value); vr_fail("render target allocation failed"); }
    // Validate old borrowed dependencies before replacing it. Failure leaves the
    // old target intact and releases the newly allocated GPU storage.
    for(VRDependency *d=vr_dependencies;d;d=d->next)
        if(t->texture && vr_descendant(d->to,t->texture) && !vr_descendant(d->from,t->texture)) { UnloadRenderTexture(value); vr_fail("raw render attachment is still referenced; use the managed texture view"); }
    RenderTexture *copy=vr_alloc(sizeof(value)); *copy=value;
    int64_t next=vr_handle(copy,"RenderTexture",1,sizeof(value),vr_drop_UnloadRenderTexture,1,h).integer;
    if(t->texture) vr_release(t->texture,NULL);
    t->texture=next;
    for(VRModelTexture *b=s->models;b;b=b->next) if(b->texture==h) {
        Model *m=vr_checked(vr_int(b->model),"Model",1);
        SetMaterialTexture(&m->materials[b->material],MATERIAL_MAP_DIFFUSE,copy->texture);
    }
}
static void vr_sampler_refresh(int64_t session,int64_t shader) {
    VRSession *s=vr_session(session); Shader *value=vr_checked(vr_int(shader),"Shader",1);
    for(VRUniform *u=s->uniforms;u;u=u->next)
        if(u->shader==shader && u->sampler && u->location>=0) SetShaderValueTexture(*value,u->location,vr_managed_texture(session,u->sampler));
}
static void vr_session_drop(void *data,size_t count) {
    (void)count; VRSession *s=data; int64_t session=vr_managed_session;
    vr_finish(s,0);
    // All application resources close together; discard their outgoing edges,
    // including sharing cycles, without disturbing unrelated raw resources.
    VRDependency **edge=&vr_dependencies;
    while(*edge) { VRDependency *d=*edge; if(vr_session_of(d->from)==session) { *edge=d->next; free(d); } else edge=&d->next; }
    for(size_t i=0;i<vr_capacity;++i) {
        VRSlot *r=&vr_slots[i];
        if(r->data && !r->owner && r->session==session && (!strcmp(r->type,"Model") || !strcmp(r->type,"Material") || !strcmp(r->type,"GL:Framebuffer"))) vr_release(vr_id(i),NULL);
    }
    for(size_t i=vr_capacity;i>0;--i) {
        VRSlot *r=&vr_slots[i-1]; if(r->data && !r->owner && r->session==session) vr_release(vr_id(i-1),NULL);
    }
    while(s->assets) { VRAsset *p=s->assets; s->assets=p->next; free(p->key); free(p); }
    while(s->uniforms) { VRUniform *p=s->uniforms; s->uniforms=p->next; free(p->name); free(p); }
    while(s->models) { VRModelTexture *p=s->models; s->models=p->next; free(p); }
    free(s->root); vr_managed_session=0;
    if(s->opened && IsWindowReady()) { EnableCursor(); CloseWindow(); }
    if(s->audio && IsAudioDeviceReady()) CloseAudioDevice();
}
static VR vr_managed_helper(int id,const VR *a,size_t count) {
    // Fixed arities also protect direct neutral-bridge clients.
    int expected=id==-15||id==-18||id==-30||id==-31 ? 1 : id==-16||id==-17||id==-19||id==-22||id==-28||id==-34 ? 2 :
        id==-20||id==-27||id==-41 ? 3 : id==-21||id==-24||id==-26||id==-32||id==-33||id==-35||id==-36||id==-42 ? 4 : id==-23 ? 5 : id==-25 ? 6 : -1;
    if(expected<0 || count!=(size_t)expected) vr_fail("incorrect managed helper argument count");
    vr_function="managed";
    if(id==-15) {
        if(vr_managed_session || IsWindowReady() || IsAudioDeviceReady()) vr_fail("an application already owns the raylib contexts");
        VRSession *s=vr_alloc(sizeof(*s)); s->root=vr_string_copy(vr_required_string(a[0])); s->last_tick=clock();
        VR h=vr_handle(s,"Session",1,sizeof(*s),vr_session_drop,1,0); vr_managed_session=h.integer; return h;
    }
    int64_t session=vr_integer(a[0]);
    if(id==-17) { int64_t h=vr_integer(a[1]); return vr_bool(session==vr_managed_session && vr_alive(session) && (!h || (vr_alive(h) && vr_session_of(h)==session))); }
    if(id==-18) { if(vr_alive(session)) { if(session!=vr_managed_session) vr_fail("invalid application session"); vr_release(session,NULL); } return vr_bool(1); }
    VRSession *s=vr_session(session); if(!s) vr_fail("application is closed");
    if(id==-16) { vr_adopt(session,vr_integer(a[1])); return a[1]; }
    if(id==-19) {
        const char *path=vr_required_string(a[1]);
        if(IsPathAbsolute(path) || !*s->root) return vr_text(path);
        size_t n=strlen(s->root)+strlen(path)+2; char *p=vr_temp(n); snprintf(p,n,"%s/%s",s->root,path); return vr_text(p);
    }
    if(id==-20 || id==-21) {
        const char *key=vr_required_string(a[1]), *kind=vr_required_string(a[2]);
        for(VRAsset *p=s->assets;p;p=p->next) if(!strcmp(p->key,key) && !strcmp(p->kind,kind)) return vr_int(p->handle);
        if(id==-20) return vr_int(0);
        int64_t h=vr_integer(a[3]); vr_adopt(session,h);
        VRAsset *p=vr_alloc(sizeof(*p)); p->key=vr_string_copy(key); p->kind=vr_canonical_type(kind); p->handle=h; p->next=s->assets; s->assets=p; return vr_int(h);
    }
    if(id==-22) {
        int64_t h=vr_integer(a[1]); if(!vr_alive(h)) return vr_bool(0); (void)vr_managed_resource(session,h);
        for(int i=0;i<s->depth;++i) if(s->passes[i].handle==h) vr_fail("cannot close a resource in an active render pass");
        vr_release(h,NULL); return vr_bool(1);
    }
    if(id==-25) {
        if(s->opened || IsWindowReady()) vr_fail("application window is already open");
        int w=vr_i32(a[1]),h=vr_i32(a[2]),fps=vr_i32(a[4]); if(w<1||h<1||fps<0) vr_fail("invalid window size or FPS");
        SetConfigFlags(vr_u32(a[5])); InitWindow(w,h,vr_required_string(a[3])); s->opened=IsWindowReady();
        if(!s->opened) vr_fail("window initialization failed"); SetTargetFPS(fps); return vr_bool(1);
    }
    if(id==-24) {
        if(!s->opened) vr_fail("render targets require an application window");
        VRTarget *t=vr_alloc(sizeof(*t)); t->follow=vr_integer(a[3])!=0;
        VR h=vr_handle(t,"Target",1,sizeof(*t),NULL,1,0); vr_adopt(session,h.integer);
        vr_resize_target(session,h.integer,vr_i32(a[1]),vr_i32(a[2])); return h;
    }
    if(id==-32) { vr_resize_target(session,vr_integer(a[1]),vr_i32(a[2]),vr_i32(a[3])); return vr_bool(1); }
    if(id==-27) {
        const char *key=vr_required_string(a[1]);
        if(!strcmp(key,"assets")) { char *root=vr_string_copy(vr_required_string(a[2])); free(s->root); s->root=root; }
        else if(!strcmp(key,"limit")) { s->limit=vr_integer(a[2]); if(s->limit<0) vr_fail("negative frame limit"); }
        else if(!strcmp(key,"stop")) s->stopped=vr_integer(a[2])!=0;
        else if(!strcmp(key,"fps")) { int fps=vr_i32(a[2]); if(fps<0) vr_fail("negative FPS"); SetTargetFPS(fps); }
        else vr_fail("unknown application setting"); return vr_bool(1);
    }
    if(id==-28) {
        const char *key=vr_required_string(a[1]);
        if(!strcmp(key,"dt")) return vr_real(s->dt);
        if(!strcmp(key,"elapsed")) return vr_real(s->elapsed);
        if(!strcmp(key,"frames")) return vr_int(s->frames);
        if(!strcmp(key,"width")) return vr_int(s->opened ? GetScreenWidth() : 0);
        if(!strcmp(key,"height")) return vr_int(s->opened ? GetScreenHeight() : 0);
        if(!strcmp(key,"window")) return vr_bool(s->opened && IsWindowReady());
        if(!strcmp(key,"audio")) return vr_bool(IsAudioDeviceReady());
        if(!strcmp(key,"passes")) return vr_int(s->depth);
        if(!strcmp(key,"uniform_queries")) return vr_int(s->uniform_queries);
        if(!strcmp(key,"resources")) { size_t n=0; for(size_t i=0;i<vr_capacity;++i) if(vr_slots[i].data && !vr_slots[i].owner && vr_slots[i].session==session) ++n; return vr_int(n); }
        vr_fail("unknown application state");
    }
    if(id==-30) {
        vr_finish(s,1);
        if(s->stopped || (s->limit && s->frames>=s->limit) || (s->opened && (!IsWindowReady() || WindowShouldClose()))) return vr_bool(0);
        if(s->opened) {
            for(size_t i=0;i<vr_capacity;++i) if(vr_slots[i].data && vr_slots[i].session==session && !strcmp(vr_slots[i].type,"Target")) {
                VRTarget *t=vr_slots[i].data; if(t->follow) vr_resize_target(session,vr_id(i),GetScreenWidth(),GetScreenHeight());
            }
            s->dt=fmin(GetFrameTime(),0.05);
            for(VRModelTexture *b=s->models;b;b=b->next) {
                Model *m=vr_checked(vr_int(b->model),"Model",1);
                SetMaterialTexture(&m->materials[b->material],MATERIAL_MAP_DIFFUSE,vr_managed_texture(session,b->texture));
            }
        } else { clock_t now=clock(); s->dt=fmin((double)(now-s->last_tick)/CLOCKS_PER_SEC,0.05); s->last_tick=now; }
        s->elapsed+=s->dt;
        for(size_t i=0;i<vr_capacity;++i) if(vr_slots[i].data && vr_slots[i].session==session && !strcmp(vr_slots[i].type,"Music")) UpdateMusicStream(*(Music *)vr_slots[i].data);
        s->frame=1; return vr_bool(1);
    }
    if(id==-31) { vr_finish(s,1); return vr_bool(1); }
    if(id==-26) {
        int kind=vr_i32(a[1]); int64_t h=kind==4 ? 0 : vr_integer(a[2]);
        if(!vr_integer(a[3])) { if(!s->depth || s->passes[s->depth-1].kind!=kind || s->passes[s->depth-1].handle!=h) vr_fail("render passes must end in LIFO order"); vr_pass_end(s); return vr_bool(1); }
        if(!s->frame || !s->opened || kind<0 || kind>4 || s->depth==32) vr_fail("render pass requires an active application frame");
        for(int i=0;i<s->depth;++i) if(s->passes[i].kind==kind) vr_fail("this render pass cannot be nested");
        if(kind==0 && s->depth) vr_fail("window drawing must be the outermost pass");
        if(kind==1 && s->depth && !(s->depth==1 && s->passes[0].kind==0)) vr_fail("render target cannot begin inside a camera, shader, or clip pass");
        if(kind>=2 && (!s->depth || (s->passes[0].kind!=0 && s->passes[0].kind!=1))) vr_fail("camera, shader, and clip passes require a drawing surface");
        if(kind && kind!=4) (void)vr_managed_resource(session,h);
        switch(kind) {
        case 0: BeginDrawing(); break;
        case 1: { VRTarget *t=vr_checked(vr_int(h),"Target",1); BeginTextureMode(*(RenderTexture *)vr_checked(vr_int(t->texture),"RenderTexture",1)); break; }
        case 2: BeginMode3D(*(Camera3D *)vr_checked(vr_int(h),"Camera3D",1)); break;
        case 3: BeginShaderMode(*(Shader *)vr_checked(vr_int(h),"Shader",1)); vr_sampler_refresh(session,h); break;
        case 4: { Rectangle r=vr_unpack_Rectangle(a[2]); BeginScissorMode((int)r.x,(int)r.y,(int)r.width,(int)r.height); break; }
        }
        s->passes[s->depth++]=(VRPass){kind,h}; return vr_bool(1);
    }
    if(id==-33) {
        Texture t=vr_managed_texture(session,vr_integer(a[1]));
        DrawTexturePro(t,(Rectangle){0,0,(float)t.width,-(float)t.height},(Rectangle){0,0,(float)vr_number(a[2]),(float)vr_number(a[3])},(Vector2){0,0},0,WHITE); return vr_bool(1);
    }
    if(id==-34) return vr_int(vr_target_texture(session,vr_integer(a[1])));
    if(id==-35) {
        int64_t h=vr_integer(a[1]); Texture t=vr_managed_texture(session,h);
        if(!strcmp(vr_slot(h)->type,"Target")) DrawTextureRec(t,(Rectangle){0,0,(float)t.width,-(float)t.height},vr_unpack_Vector2(a[2]),vr_unpack_Color(a[3]));
        else DrawTextureV(t,vr_unpack_Vector2(a[2]),vr_unpack_Color(a[3]));
        return vr_bool(1);
    }
    if(id==-36) {
        int w=vr_i32(a[1]),h=vr_i32(a[2]); if(w<1||h<1||w>16384||h>16384) vr_fail("invalid image dimensions");
        Image *p=vr_alloc(sizeof(*p)); *p=GenImageColor(w,h,vr_unpack_Color(a[3]));
        VR out=vr_handle(p,"Image",1,sizeof(*p),vr_drop_UnloadImage,1,0); vr_adopt(session,out.integer); return out;
    }
    if(id==-41) { Texture t=vr_managed_texture(session,vr_integer(a[1])); return vr_int(vr_integer(a[2]) ? t.height : t.width); }
    if(id==-42) {
        int64_t model=vr_integer(a[1]),texture=vr_integer(a[2]); int material=vr_i32(a[3]);
        (void)vr_managed_resource(session,model); Model *m=vr_checked(vr_int(model),"Model",1);
        if(material<0 || material>=m->materialCount) vr_fail("model material index is out of bounds");
        Texture t=vr_managed_texture(session,texture);
        VRModelTexture *b=s->models; while(b && (b->model!=model || b->material!=material)) b=b->next;
        if(!b) { b=vr_alloc(sizeof(*b)); b->model=model; b->material=material; b->next=s->models; s->models=b; }
        b->texture=texture; SetMaterialTexture(&m->materials[material],MATERIAL_MAP_DIFFUSE,t);
        vr_depend(model,texture,material*2); return vr_bool(1);
    }
    if(id==-23) {
        int64_t shader=vr_integer(a[1]); (void)vr_managed_resource(session,shader);
        Shader *value=vr_checked(vr_int(shader),"Shader",1); const char *name=vr_required_string(a[2]); int mode=vr_i32(a[4]);
        VRUniform *u=s->uniforms;
        while(u && (u->shader!=shader || strcmp(u->name,name))) u=u->next;
        if(!u) { u=vr_alloc(sizeof(*u)); u->shader=shader; u->name=vr_string_copy(name); u->location=GetShaderLocation(*value,name); ++s->uniform_queries; u->next=s->uniforms; s->uniforms=u; }
        if(u->location<0) vr_fail("shader uniform was not found");
        if(mode==2) { int64_t texture=vr_integer(a[3]); Texture t=vr_managed_texture(session,texture); vr_depend(shader,texture,1000+u->location); SetShaderValueTexture(*value,u->location,t); u->sampler=texture; return vr_bool(1); }
        u->sampler=0;
        VRDependency **edge=&vr_dependencies;
        while(*edge) { VRDependency *d=*edge; if(d->from==shader && d->key==1000+u->location) { *edge=d->next; free(d); } else edge=&d->next; }
        VR v=a[3];
        if(v.kind==VR_RECORD && v.count==16) { SetShaderValueMatrix(*value,u->location,vr_unpack_Matrix(v)); return vr_bool(1); }
        size_t n=v.kind==VR_LIST||v.kind==VR_RECORD ? v.count : 1;
        if(n<1||n>4) vr_fail("uniform requires a scalar, vector of 1..4 values, or Matrix");
        float floats[4]={0}; int ints[4]={0};
        int integral=mode==1 || (n==1 && (v.kind==VR_INT||v.kind==VR_BOOL));
        const char *fields[]={"x","y","z","w"};
        for(size_t i=0;i<n;++i) { VR item=v.kind==VR_RECORD ? vr_member(v,fields[i],i) : n==1 && v.kind!=VR_LIST ? v : v.items[i]; if(integral) ints[i]=vr_i32(item); else floats[i]=(float)vr_number(item); }
        SetShaderValue(*value,u->location,integral ? (const void *)ints : (const void *)floats, (integral ? SHADER_UNIFORM_INT : SHADER_UNIFORM_FLOAT)+(int)n-1);
        return vr_bool(1);
    }
    vr_fail("unknown managed helper"); return vr_null();
}
