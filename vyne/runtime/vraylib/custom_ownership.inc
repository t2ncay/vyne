// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
// Model importers load textures internally; UnloadModel intentionally leaves
// them for the caller. Adopt each unique non-default texture under this model.
static void vr_adopt_materials(int64_t handle,Material *materials,size_t count) {
    if(!handle) return;
    for(size_t i=0;i<count;++i) {
        if(!materials[i].maps) continue;
        for(int j=0;j<=MATERIAL_MAP_BRDF;++j) {
            Texture t=materials[i].maps[j].texture;
            if(!t.id || t.id==rlGetTextureIdDefault()) continue;
            int found=0;
            for(size_t k=0;k<vr_capacity;++k)
                if(vr_slots[k].data && vr_slots[k].owner==handle && !strcmp(vr_slots[k].type,"Texture") && ((Texture *)vr_slots[k].data)->id==t.id) found=1;
            if(!found) {
                Texture *copy=vr_alloc(sizeof(t)); *copy=t;
                vr_handle(copy,"Texture",1,sizeof(t),vr_drop_UnloadTexture,1,handle);
            }
        }
    }
}
static void vr_adopt_model(int64_t handle) {
    Model *m=vr_checked(vr_int(handle),"Model",1);
    vr_adopt_materials(handle,m->materials,m->materialCount);
}

// rlgl has no standalone renderbuffer-delete entry point. A temporary FBO's
// native destructor owns and deletes its depth renderbuffer attachment.
static void vr_drop_renderbuffer(void *data,size_t count) {
    (void)count;
    unsigned int fbo=rlLoadFramebuffer();
    rlFramebufferAttach(fbo,*(unsigned int *)data,RL_ATTACHMENT_DEPTH,RL_ATTACHMENT_RENDERBUFFER,0);
    rlUnloadFramebuffer(fbo);
}
