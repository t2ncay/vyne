#!/usr/bin/env python3
"""Generate reproducible raylib/raymath/rlgl bindings from vendored 6.2 headers.

No network or third-party Python dependencies. Handwritten ownership policy is
kept here; generated files are checked in and verified with --check in CI.
"""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "vyne/runtime/vraylib"
SCALARS = {"bool", "char", "signed char", "unsigned char", "short", "unsigned short",
           "int", "unsigned int", "long", "unsigned long", "float", "double", "size_t"}
RESOURCES = {"Image", "Texture", "RenderTexture", "Font", "Mesh", "Shader", "Material", "GlyphInfo",
             "Model", "ModelAnimation", "Wave", "AudioStream", "Sound", "Music",
             "FilePathList", "AutomationEventList", "rlRenderBatch", "ModelSkeleton", "rlVertexBuffer"}
DTORS = {"Image": "UnloadImage", "Texture": "UnloadTexture", "RenderTexture": "UnloadRenderTexture",
         "Font": "UnloadFont", "Mesh": "UnloadMesh", "Shader": "UnloadShader", "Material": "UnloadMaterial",
         "Model": "UnloadModel", "Wave": "UnloadWave",
         "AudioStream": "UnloadAudioStream", "Sound": "UnloadSound", "Music": "UnloadMusicStream",
         "FilePathList": "UnloadDirectoryFiles", "AutomationEventList": "UnloadAutomationEventList",
         "rlRenderBatch": "rlUnloadRenderBatch"}
POINTER_OWNERS = {
    "LoadRandomSequence": ("UnloadRandomSequence", "count"),
    "LoadFileData": ("UnloadFileData", "dataSize"),
    "LoadFileText": ("UnloadFileText", "strlen"),
    "CompressData": ("MemFree", "compDataSize"), "DecompressData": ("MemFree", "dataSize"),
    "EncodeDataBase64": ("MemFree", "outputSize"), "DecodeDataBase64": ("MemFree", "outputSize"),
    "LoadImageColors": ("UnloadImageColors", "image_pixels"),
    "LoadImagePalette": ("UnloadImagePalette", "colorCount"),
    "LoadWaveSamples": ("UnloadWaveSamples", "wave_samples"),
    "LoadCodepoints": ("UnloadCodepoints", "count"), "LoadUTF8": ("UnloadUTF8", "strlen"),
    "LoadFontData": ("UnloadFontData", "glyphCount"),
    "LoadMaterials": ("UnloadMaterials", "materialCount"),
    "LoadModelAnimations": ("UnloadModelAnimations", "animCount"),
    "TextReplaceAlloc": ("MemFree", "strlen"), "TextInsertAlloc": ("MemFree", "strlen"),
    "TextReplaceBetweenAlloc": ("MemFree", "strlen"),
    "LoadTextLines": ("UnloadTextLines", "count"),
    "ExportImageToMemory": ("MemFree", "fileSize"),
    "MemAlloc": ("MemFree", "size"), "MemRealloc": ("MemFree", "size"),
    "rlReadScreenPixels": ("MemFree", "screen_pixels"),
    "rlReadTexturePixels": ("MemFree", "texture_bytes"),
}
HELPERS = {"buffer": (-1, 2), "buffer_read": (-2, 2), "buffer_write": (-3, 3),
           "release": (-4, 1), "field": (-5, 3), "set_field": (-6, 3),
           "live_handles": (-7, 0), "shutdown": (-8, 0), "model_texture": (-9, 3),
           "model_shader": (-10, 2), "render_texture": (-11, 1),
           "buffer_size": (-12, 1), "valid": (-13, 1), "environment": (-14, 1)}
HELPERS.update({
    "managed_session": (-15,1), "managed_adopt": (-16,2), "managed_valid": (-17,2),
    "managed_close": (-18,1), "managed_path": (-19,2), "managed_cache": (-20,3),
    "managed_store": (-21,4), "managed_release": (-22,2), "managed_uniform": (-23,5),
    "managed_target": (-24,4), "managed_open": (-25,6), "managed_pass": (-26,4),
    "managed_config": (-27,3), "managed_state": (-28,2), "managed_frame": (-30,1),
    "managed_finish": (-31,1), "managed_resize": (-32,4), "managed_present": (-33,4),
    "managed_texture": (-34,2), "managed_draw_texture": (-35,4), "managed_image": (-36,4),
    "managed_texture_size": (-41,3),
    "managed_model_texture": (-42,4),
})
GPU_OWNERS = {
    "rlLoadVertexArray": ("rlUnloadVertexArray", "VertexArray"),
    "rlLoadVertexBuffer": ("rlUnloadVertexBuffer", "VertexBuffer"),
    "rlLoadVertexBufferElement": ("rlUnloadVertexBuffer", "VertexBuffer"),
    "rlLoadTexture": ("rlUnloadTexture", "Texture"),
    "rlLoadTextureCubemap": ("rlUnloadTexture", "Texture"),
    "rlLoadTextureDepth": ("rlUnloadTexture", "Texture"),
    "rlLoadFramebuffer": ("rlUnloadFramebuffer", "Framebuffer"),
    "rlLoadShader": ("rlUnloadShader", "Shader"),
    "rlLoadShaderProgram": ("rlUnloadShaderProgram", "ShaderProgram"),
    "rlLoadShaderProgramEx": ("rlUnloadShaderProgram", "ShaderProgram"),
}
BLOCKED = {"rlLoadExtensions": "OpenGL loader callbacks are managed by InitWindow",
           "rlglInit": "rlgl lifecycle is managed by InitWindow",
           "rlglClose": "rlgl lifecycle is managed by CloseWindow",
           "rlCopyFramebuffer": "requires the software graphics backend",
           "rlResizeFramebuffer": "requires the software graphics backend"}
for _name in ("rlComputeShaderDispatch", "rlLoadShaderBuffer", "rlUnloadShaderBuffer", "rlUpdateShaderBuffer",
              "rlBindShaderBuffer", "rlReadShaderBuffer", "rlCopyShaderBuffer", "rlGetShaderBufferSize",
              "rlBindImageTexture", "rlLoadShaderProgramCompute"):
    BLOCKED[_name] = "requires OpenGL 4.3; the bundled desktop build uses OpenGL 3.3"


def clean(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def snake(name):
    if name.startswith("rl") and len(name) > 2 and name[2].isupper():
        name = name[2:]
    name = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", name)
    result = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name).lower()
    return re.sub(r"([a-z])(\d)_d\b", r"\1_\2d", result)

def vyfield(name):
    return "event_type" if name == "type" else name


def declaration(s):
    s = re.sub(r"\s+", " ", s.strip())
    m = re.match(r"(.*?)\b([A-Za-z_]\w*)\s*(\[([^]]+)\])?$", s)
    if not m:
        raise ValueError(f"cannot parse declaration: {s}")
    return {"type": m[1].strip(), "name": m[2], "array": m[4]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    opts = parser.parse_args()
    headers = {n: clean((ROOT / f"vendor/raylib/src/{n}.h").read_text()) for n in ("raylib", "raymath", "rlgl")}
    headers["rlgl"] = headers["rlgl"].split("#endif // RLGL_H")[0] if "#endif // RLGL_H" in headers["rlgl"] else headers["rlgl"].split("#if defined(RLGL_IMPLEMENTATION)")[0]
    structs, aliases, functions, constants = {}, {}, [], []
    for source, text in headers.items():
        for tag, body, name in re.findall(r"typedef\s+struct\s+(\w+)\s*\{(.*?)\}\s*(\w+)\s*;", text, re.S):
            if name in structs:
                continue
            body = re.sub(r"^\s*#.*$", "", body, flags=re.M)
            fields = []
            for line in body.split(";"):
                line = line.strip()
                if not line:
                    continue
                if "," in line:
                    parts = line.split(",")
                    first = declaration(parts[0]); fields.append(first)
                    fields.extend(declaration(first["type"] + " " + p) for p in parts[1:])
                else:
                    f = declaration(line)
                    if not any(old["name"] == f["name"] for old in fields):
                        fields.append(f)
            structs[name] = fields
        for raw in re.findall(r"typedef\s+([^;{}()]+);", text):
            a = declaration(raw)
            if not a["type"].startswith("struct "):
                aliases[a["name"]] = a["type"]
        macro = "RMAPI" if source == "raymath" else "RLAPI"
        for ret, name, params in re.findall(r"^" + macro + r"\s+([\w\s*]+?)\b(\w+)\s*\(([^)]*)\)\s*[;{]", text, re.M):
            args = [] if params.strip() in ("", "void") else [declaration(p) if p.strip() != "..." else {"type": "...", "name": "varargs", "array": None} for p in params.split(",")]
            functions.append({"source": source, "name": name, "return": ret.strip(), "args": args})
        for body in re.findall(r"typedef\s+enum\s*(?:\w+)?\s*\{(.*?)\}\s*\w*\s*;", text, re.S):
            for part in body.split(","):
                m = re.match(r"\s*(\w+)", part)
                if m and m[1] not in constants:
                    constants.append(m[1])
    for name in re.findall(r"^\s*#define\s+(RL_[A-Z0-9_]+)\s+[0-9][^\n]*$", headers["rlgl"], re.M):
        if name not in constants: constants.append(name)

    def normalize(t):
        t = re.sub(r"\bconst\b", "", t)
        t = re.sub(r"\s+", " ", t).strip().replace(" *", "*")
        base = t.replace("*", "").strip(); depth = t.count("*")
        seen = set()
        while base in aliases and base not in seen:
            seen.add(base); target = aliases[base]
            depth += target.count("*"); base = target.replace("*", "").strip()
        return base, depth

    # Opaque audio structs are never dereferenced from the public bridge.
    opaque = {"rAudioBuffer", "rAudioProcessor"}
    all_types = SCALARS | set(structs)
    def ctype(t):
        b, d = normalize(t)
        return b + "*" * d

    def unpack(t, v):
        b, depth = normalize(t)
        if depth:
            if b == "char" and depth == 1:
                return f"({ctype(t)})vr_chars({v})"
            return f"({ctype(t)})vr_pointer({v}, \"{b if depth == 1 else b + '*' * (depth - 1)}\", 0)"
        if b in structs:
            return f"vr_unpack_{b}({v})"
        if b == "float" or b == "double": return f"({b})vr_number({v})"
        if b == "bool": return f"(bool)vr_integer({v})"
        if b == "unsigned int": return f"vr_u32({v})"
        if b == "int": return f"vr_i32({v})"
        return f"({b})vr_integer({v})"

    def pack(t, expression, owner="0"):
        b, depth = normalize(t)
        if depth:
            return f"vr_handle((void *)({expression}), \"{b if depth == 1 else b + '*' * (depth - 1)}\", 0, {('sizeof(' + b + ')') if b in all_types and depth == 1 else 'sizeof(void *)'}, NULL, 0, {owner})"
        if b in structs: return f"vr_pack_{b}({expression}, {owner})"
        return (f"vr_real({expression})" if b in ("float", "double") else f"vr_bool({expression})" if b == "bool" else f"vr_int({expression})")

    # Type converters and reflection. Resource values are checked handles; PODs
    # accept named Vyne structs or positional arrays, including nested values.
    types = ["// Generated by scripts/generate_vraylib.py; do not edit."]
    for name in structs:
        types += [f"static {name} vr_unpack_{name}(VR);", f"static VR vr_pack_{name}({name}, int64_t);"]
    types += ["static size_t vr_type_size(const char *);", "static VR vr_read_type(const char *, const void *, int64_t);", "static void vr_write_type(const char *, void *, VR);", "static void *vr_pointer(VR,const char *,size_t);", "static char *vr_chars(VR);"]
    canonical = {n: n for n in all_types | {"void"}}
    for alias in aliases:
        base, depth = normalize(alias)
        if base in all_types and depth <= 2:
            canonical[alias] = base + "*" * depth
    for name, target in list(canonical.items()):
        for depth in (1, 2):
            if target.count("*") + depth <= 2:
                canonical[name + "*" * depth] = target + "*" * depth
    types += ["static const char *vr_canonical_type(const char *name) {"]
    for name, target in sorted(canonical.items()):
        types += [f'    if(!strcmp(name,"{name}")) return "{target}";']
    types += ['    vr_fail("unknown native buffer element type"); return NULL;', "}"]
    for name, fields in structs.items():
        types += [f"static {name} vr_unpack_{name}(VR v) {{", f"    if (v.kind == VR_INT || v.kind == VR_HANDLE) {{ {name} *p = vr_checked(v, \"{name}\", 1); if (!p) vr_fail(\"null struct handle\"); return *p; }}"]
        if name in RESOURCES:
            types += ["    vr_fail(\"resource arguments must be native handles\");", f"    return ({name}){{0}};", "}"]
        else:
            types += [f"    {name} result = {{0}};"]
            for i, f in enumerate(fields):
                value = f'vr_member(v, "{vyfield(f["name"])}", {i})'
                if f["array"]:
                    types += [f"    {{ VR a = {value}; if (a.count != {f['array']}) vr_fail(\"incorrect fixed-array length\");", f"      for (size_t j=0; j<{f['array']}; ++j) result.{f['name']}[j] = {unpack(f['type'], 'a.items[j]')}; }}"]
                else:
                    types += [f"    result.{f['name']} = {unpack(f['type'], value)};"]
            types += ["    return result;", "}"]
        types += [f"static VR vr_pack_{name}({name} value, int64_t owner) {{"]
        if name in RESOURCES:
            types += [f"    {name} *copy = vr_alloc(sizeof(value)); *copy = value;", f"    return vr_handle(copy, \"{name}\", 1, sizeof(value), NULL, 1, owner);", "}"]
        else:
            types += [f"    VR result = vr_list({len(fields)}, \"vraylib.{name}\");"]
            for i, f in enumerate(fields):
                if f["array"]:
                    types += [f"    result.items[{i}] = vr_list({f['array']}, NULL);", f"    for (size_t j=0; j<{f['array']}; ++j) result.items[{i}].items[j] = {pack(f['type'], 'value.' + f['name'] + '[j]', 'owner')};"]
                else:
                    types += [f"    result.items[{i}] = {pack(f['type'], 'value.' + f['name'], 'owner')};"]
                types += [f"    result.items[{i}].name = \"{vyfield(f['name'])}\";"]
            types += ["    return result;", "}"]
    types += ["static int vr_resource_type(const char *t) { return " + " || ".join(f'!strcmp(t,"{n}")' for n in sorted(RESOURCES)) + "; }"]
    types += ["static size_t vr_type_size(const char *t) {"]
    for n in sorted(all_types): types += [f'    if (!strcmp(t,"{n}")) return sizeof({n});']
    types += ['    if (!strcmp(t,"void")) return 1;', '    if (strchr(t,\'*\')) return sizeof(void *);', '    vr_fail("unknown buffer element type"); return 0;', "}"]
    types += ["static VR vr_read_type(const char *t, const void *p, int64_t owner) {"]
    for n in sorted(all_types):
        expr = f"*(({n} const *)p)"
        if n in RESOURCES:
            types += [f'    if (!strcmp(t,"{n}")) return vr_handle((void *)p, "{n}", 1, sizeof({n}), NULL, 0, owner);']
        else:
            types += [f'    if (!strcmp(t,"{n}")) return {pack(n, expr, "owner")};']
    types += ['    if(!strcmp(t,"void")) return vr_int(*(const unsigned char *)p);',
              '    if(strchr(t,\'*\')) {',
              '        void *data=*(void *const *)p; if(!data) return vr_int(0);',
              '        if(!strcmp(t,"char*")) return vr_text((const char *)data);',
              '        for(size_t i=0;i<vr_capacity;++i) if(vr_slots[i].data==data) return vr_int(vr_id(i));',
              '        vr_fail("pointer output requires a known buffer extent");', '    }',
              '    vr_fail("opaque pointer data cannot be read"); return vr_null();', "}"]
    types += ["static void vr_write_type(const char *t, void *p, VR v) {"]
    for n in sorted(all_types): types += [f'    if (!strcmp(t,"{n}")) {{ *(({n} *)p) = {unpack(n,"v")}; return; }}']
    types += ['    if(!strcmp(t,"void")) { *(unsigned char *)p=(unsigned char)vr_integer(v); return; }',
              '    if(!strcmp(t,"char*") && v.kind==VR_TEXT) { size_t n=strlen(v.text)+1; char *s=vr_temp(n); memcpy(s,v.text,n); *(char **)p=s; return; }',
              '    if(strchr(t,\'*\')) {',
              '        if(v.kind==VR_NULL || !vr_integer(v)) { *(void **)p=NULL; return; }',
              '        VRSlot *s=vr_slot(vr_integer(v)); *(void **)p=s->data; return;', '    }',
              '    vr_fail("opaque pointer data cannot be written");', "}"]
    types += ["static void *vr_pointer(VR v, const char *type, size_t count) {",
              "    if (v.kind == VR_LIST) {",
              "        if (v.count < count) vr_fail(\"input array is too short\");",
              "        size_t stride = vr_type_size(type);",
              "        if (v.count > SIZE_MAX / stride) vr_fail(\"input array is too large\");",
              "        char *p = vr_temp(v.count * stride);",
              "        for (size_t i=0; i<v.count; ++i) vr_write_type(type,p+i*stride,v.items[i]);",
              "        return p;", "    }", "    return vr_checked(v,type,count);", "}",
              "static char *vr_chars(VR v) {",
              "    if (v.kind == VR_TEXT) { size_t n=strlen(v.text)+1; char *s=vr_temp(n); memcpy(s,v.text,n); return s; }",
              '    return vr_checked(v,"char",0);', "}"]
    for n, destructor in DTORS.items():
        prefix = "rlSetRenderBatchActive(NULL); " if destructor == "rlUnloadRenderBatch" else ""
        if destructor=="UnloadMaterial":
            prefix = "Material *m=p; m->shader.id=rlGetShaderIdDefault(); if(m->maps) for(int i=0;i<=MATERIAL_MAP_BRDF;++i) m->maps[i].texture.id=0; "
        types += [f"static void vr_drop_{destructor}(void *p, size_t count) {{ (void)count; {prefix}{destructor}(*(({n} *)p)); }}"]
    types += ["static void vr_drop_UnloadSoundAlias(void *p, size_t n) { (void)n; UnloadSoundAlias(*(Sound *)p); }"]
    for destructor in sorted({d for d, _ in GPU_OWNERS.values()}):
        prefix = "rlFramebufferAttach(*(unsigned int *)p,0,RL_ATTACHMENT_DEPTH,RL_ATTACHMENT_TEXTURE2D,0); " if destructor=="rlUnloadFramebuffer" else ""
        types += [f"static void vr_gpu_drop_{destructor}(void *p,size_t n) {{ (void)n; {prefix}{destructor}(*(unsigned int *)p); }}"]
    pointer_drop = {}
    fn_lookup = {f["name"]: f for f in functions}
    for fn, (destructor, rule) in POINTER_OWNERS.items():
        if fn not in fn_lookup: continue
        if destructor == "MemFree": pointer_drop[fn] = "vr_memory_drop"; continue
        # Some unload APIs use count as a second argument.
        if destructor not in fn_lookup:
            # raylib 6.2 LoadMaterials has no collective unload API.
            if destructor == "UnloadMaterials":
                types += ['static void vr_drop_materials(void *p,size_t n) { Material *a=p; for(size_t i=0;i<n;++i) vr_drop_UnloadMaterial(&a[i],1); MemFree(p); }']
                pointer_drop[fn] = "vr_drop_materials"
                continue
            raise ValueError(f"missing pointer destructor {destructor}")
        args = fn_lookup[destructor]["args"]
        extra = ", (int)count" if len(args) > 1 else ""
        cast = ctype(args[0]["type"])
        types += [f"static void vr_ptr_drop_{destructor}(void *p,size_t count) {{ (void)count; {destructor}(({cast})p{extra}); }}"]
        pointer_drop[fn] = f"vr_ptr_drop_{destructor}"

    # Public struct inspection with bounded arrays and parent-linked borrows.
    lengths = {("Model", "meshes"): "s->meshCount", ("Model", "materials"): "s->materialCount",
               ("Model", "meshMaterial"): "s->meshCount", ("Font", "glyphs"): "s->glyphCount",
               ("Font", "recs"): "s->glyphCount", ("FilePathList", "paths"): "s->count",
               ("AutomationEventList", "events"): "s->count", ("Material", "maps"): "MATERIAL_MAP_BRDF+1",
               ("Shader", "locs"): "RL_MAX_SHADER_LOCATIONS", ("ModelSkeleton", "bones"): "s->boneCount",
               ("ModelSkeleton", "bindPose"): "s->boneCount", ("ModelAnimation", "keyframePoses"): "s->keyframeCount",
               ("Model", "currentPose"): "s->skeleton.boneCount", ("Model", "boneMatrices"): "s->skeleton.boneCount",
               ("rlRenderBatch", "vertexBuffer"): "s->bufferCount", ("rlRenderBatch", "draws"): "RL_DEFAULT_BATCH_DRAWCALLS"}
    for field, multiplier in {"vertices": 3, "texcoords": 2, "texcoords2": 2, "normals": 3, "tangents": 4,
                              "colors": 4, "indices": 0, "boneIndices": 4, "boneWeights": 4,
                              "animVertices": 3, "animNormals": 3, "vboId": -1}.items():
        lengths["Mesh", field] = "s->triangleCount*3" if multiplier == 0 else "7" if multiplier == -1 else f"s->vertexCount*{multiplier}"
    types += ["static VR vr_get_field(VR handle,const char *field,int64_t index) {", "    VRSlot *slot=vr_slot(vr_integer(handle));", "    if(index<0) vr_fail(\"negative field index\");"]
    for n, fields in structs.items():
        types += [f'    if(!strcmp(slot->type,"{n}")) {{ {n} *s=slot->data;']
        for f in fields:
            b, d = normalize(f["type"])
            types += [f'        if(!strcmp(field,"{f["name"]}")) {{']
            if f["array"] or d:
                length = f["array"] or lengths.get((n, f["name"]), "0")
                types += [f"            if((uint64_t)index >= (uint64_t)({length}) || !s->{f['name']}) vr_fail(\"field index unavailable or out of bounds\");"]
                if b == "char" and f["array"]:
                    types += [f"            return vr_text(s->{f['name']});"]
                elif d > 1:
                    if n=="ModelAnimation" and f["name"]=="keyframePoses":
                        types += ['            return vr_handle(s->keyframePoses[index],"Transform",s->boneCount,sizeof(Transform),NULL,0,vr_integer(handle));']
                    else:
                        types += [f'            return vr_text(s->{f["name"]}[index]);' if b == "char" else '            vr_fail("nested pointer arrays require a dedicated adapter");']
                elif b in all_types:
                    types += [f'            return vr_read_type("{b}",&s->{f["name"]}[index],vr_integer(handle));']
                else:
                    types += ['            vr_fail("opaque native field");']
            else:
                types += ['            if(index) vr_fail("scalar field index must be zero");']
                types += [f'            return vr_read_type("{b}",&s->{f["name"]},vr_integer(handle));']
            types += ["        }"]
        types += ["    }"]
    types += ['    vr_fail("unknown resource field"); return vr_null();', "}"]
    types += ["static void vr_set_field(VR handle,const char *field,VR value) {", "    VRSlot *slot=vr_slot(vr_integer(handle));"]
    for n, fields in structs.items():
        types += [f'    if(!strcmp(slot->type,"{n}")) {{ {n} *s=slot->data;']
        for f in fields:
            b, d = normalize(f["type"])
            if d or f["array"] or b in RESOURCES or f["name"] in ("id", "vaoId", "ctxType", "vertexCount", "triangleCount", "boneCount", "materialCount", "meshCount", "glyphCount", "frameCount", "keyframeCount", "sampleSize", "sampleRate", "channels", "width", "height", "mipmaps", "format", "capacity", "count"):
                continue
            types += [f'        if(!strcmp(field,"{f["name"]}")) {{ s->{f["name"]}={unpack(f["type"],"value")}; return; }}']
        types += ["    }"]
    types += ['    vr_fail("field is unknown or ownership-sensitive; use its dedicated API");', "}"]

    calls = ["// Generated by scripts/generate_vraylib.py; do not edit.", "static VR vr_dispatch(int id,const VR *a,size_t count) {", "    switch(id) {"]
    maps, runtime, inventory = [], [], []
    def binding(name, ident, arity, reason=None, property=False):
        symbol = "vraylib_" + name
        params = ", ".join(f"VyneValue a{i}" for i in range(arity)) or "void"
        if arity:
            body = "VyneValue a[] = {" + ",".join(f"a{i}" for i in range(arity)) + f"}}; return vraylib_invoke({ident},{arity},a);"
        else: body = f"return vraylib_invoke({ident},0,NULL);"
        runtime.append(f"static inline VyneValue {symbol}({params}) {{ {body} }}")
        if property:
            maps.append(f'    {{"{name}","{symbol}()",true,false,nullptr,nullptr,0}},')
        else:
            maps.append(f'    {{"{name}","{symbol}",false,false,nullptr,nullptr,{arity}}},')
    for name, (ident, arity) in HELPERS.items(): binding(name, ident, arity)
    all_names = set(HELPERS)
    for ident, fn in enumerate(functions):
        n, args, source = fn["name"], fn["args"], fn["source"]
        public = ("math_" if source == "raymath" else "gl_" if source == "rlgl" else "") + snake(n)
        if public in all_names: raise ValueError(f"duplicate native name {public}")
        all_names.add(public)
        reason = BLOCKED.get(n)
        if any("Callback" in a["type"] for a in args): reason = "C callbacks cannot capture Vyne values; callback registration is unavailable"
        if any(a["type"] == "..." for a in args):
            if n not in ("TraceLog", "TextFormat"): reason = "C varargs require an explicit adapter"
            args = args[:-1]  # literal-safe adapter for TraceLog/TextFormat
        fn["vyne"] = public; fn["status"] = "unavailable" if reason else "bound"
        fn["reason"] = reason
        fn["vyne_arity"] = len(args)
        if n in ("TraceLog", "TextFormat"):
            fn["status"] = "adapted"
            fn["adapter"] = "literal text only; format in Vyne with string interpolation"
        inventory.append(fn)
        binding(public, ident, len(args), reason)
        calls += [f"    case {ident}: {{", f'        vr_function="{public}";', f'        if(count!={len(args)}) vr_fail("incorrect argument count");']
        if reason:
            calls += [f'        vr_fail("{reason}"); return vr_null();', "    }"]
            continue
        if n in ("CloseWindow", "CloseAudioDevice"):
            calls += ["        " + ("vr_close_graphics();" if n=="CloseWindow" else "vr_close_audio();") + " return vr_null();", "    }"]
            continue
        if n == "TraceLog":
            calls += ['        TraceLog(vr_i32(a[0]),"%s",vr_required_string(a[1])); return vr_null();', "    }"]; continue
        if n == "TextFormat":
            calls += ['        return vr_text(vr_required_string(a[0]));', "    }"]; continue
        gpu_drop = next((d for d, _ in GPU_OWNERS.values() if n == d), None)
        if gpu_drop:
            calls += [f"        vr_release(vr_integer(a[0]),vr_gpu_drop_{gpu_drop}); return vr_null();", "    }"]; continue
        # Unloads invalidate handles and refuse borrowed objects / wrong destructors.
        unload = next((d for d in DTORS.values() if d == n), None)
        drop_ptr = next((pointer_drop[k] for k, (d, _) in POINTER_OWNERS.items() if d == n and k in pointer_drop), None)
        if n == "MemFree": drop_ptr = None
        if unload or drop_ptr or n in ("MemFree", "UnloadSoundAlias"):
            destructor = "vr_drop_UnloadSoundAlias" if n == "UnloadSoundAlias" else "vr_drop_" + n if unload else drop_ptr or "vr_memory_drop"
            if len(args)>1:
                calls += ['        if((size_t)vr_integer(a[1]) != vr_slot(vr_integer(a[0]))->count) vr_fail("unload count does not match allocation");']
            calls += [f"        vr_release(vr_integer(a[0]),{destructor}); return vr_null();", "    }"]; continue
        for i, arg in enumerate(args):
            b, d = normalize(arg["type"])
            if d and b == "char" and d == 1 and "const" in arg["type"]:
                expr = f"vr_string(a[{i}])"
            else: expr = unpack(arg["type"], f"a[{i}]")
            calls += [f"        {arg['type']} p{i} = {expr};"]
            if d and "const" not in arg["type"] and b not in ("char", "void") and n != "SetWindowIcons":
                calls += [f'        if(a[{i}].kind==VR_LIST) vr_fail("mutable pointer arguments require a persistent typed buffer");']
        # Bound common pointer/count forms before crossing into C. Scalar output
        # pointers always require one writable element. Byte-count APIs validate
        # bytes rather than confusing a float element count with buffer capacity.
        nullable = {"LoadShader", "LoadShaderFromMemory", "SetWindowIcons", "LoadFontEx", "LoadFontFromMemory", "LoadFontData", "rlSetRenderBatchActive", "SetAutomationEventList", "rlLoadVertexBuffer", "rlLoadVertexBufferElement", "rlDrawVertexArrayElements", "rlDrawVertexArrayElementsInstanced"}
        for i, arg in enumerate(args):
            b, d = normalize(arg["type"])
            if not d: continue
            isbytes = b == "void" or b == "unsigned char"
            size = "1"
            # Count arguments immediately following an array pointer use the
            # upstream count; named scalar output pointers are excluded.
            if i+1<len(args) and not normalize(args[i+1]["type"])[1] and re.search(r"[Cc]ount|[Ss]ize|[Ll]ength", args[i+1]["name"]):
                size = f"(size_t)p{i+1}"
                calls += [f'        if(p{i+1}<0) vr_fail("negative pointer count");']
            if b=="char": size="0" if n in nullable else "1"
            if n in ("SetShaderValue", "SetShaderValueV") and i==2:
                size=f"vr_uniform_bytes(p3,{('p4' if n=='SetShaderValueV' else '1')})"; isbytes=True
            if n=="rlSetUniform" and i==1:
                size="vr_uniform_bytes(p2,p3)"; isbytes=True
            if n=="UpdateTexture" and i==1:
                size="(size_t)GetPixelDataSize(p0.width,p0.height,p0.format)"; isbytes=True
            if n=="UpdateTextureRec" and i==2:
                size="(size_t)GetPixelDataSize((int)p1.width,(int)p1.height,p0.format)"; isbytes=True
            if n in ("UpdateSound", "UpdateAudioStream") and i==1:
                size="(size_t)p2*p0.stream.channels*(p0.stream.sampleSize/8)" if n=="UpdateSound" else "(size_t)p2*p0.channels*(p0.sampleSize/8)"; isbytes=True
            if n=="rlMultMatrixf" and i==0: size="16"
            if n=="TextCopy" and i==0: size="strlen(p1)+1"
            if n=="TextAppend" and i==0: size="strlen(p0)+strlen(p1)+1"
            calls += [f"        vr_span(a[{i}],{size},{int(isbytes)},{int(n in nullable)});"]
        if n=="MemRealloc":
            calls += ['        if(!p1 || !p0 || vr_slot(vr_integer(a[0]))->drop!=vr_memory_drop) vr_fail("MemRealloc requires an owned MemAlloc buffer and a positive size");']
        if n=="LoadModelFromMesh":
            calls += ['        if(vr_slot(vr_integer(a[0]))->owner || vr_slot(vr_integer(a[0]))->drop!=vr_drop_UnloadMesh) vr_fail("model construction requires an owned mesh");']
        if n=="LoadMusicStreamFromMemory":
            calls += ['        if(a[1].kind==VR_LIST) vr_fail("streamed audio requires a persistent native byte buffer");']
        params = ", ".join(f"p{i}" for i in range(len(args)))
        retbase, depth = normalize(fn["return"])
        if retbase == "void" and not depth:
            calls += [f"        {n}({params});"]
            if n=="SetMaterialTexture": calls += ['        vr_depend(vr_integer(a[0]),vr_integer(a[2]),p1);']
            if n=="rlFramebufferAttach": calls += ['        if(vr_integer(a[0])>UINT_MAX && vr_integer(a[1])>UINT_MAX) vr_depend(vr_integer(a[0]),vr_integer(a[1]),p2);']
            if n=="SetAutomationEventList": calls += ['        vr_active_automation=p0;']
            if n=="rlSetRenderBatchActive": calls += ['        vr_active_batch=p0;']
            calls += ["        return vr_null();", "    }"]; continue
        calls += [f"        {fn['return']} result = {n}({params});"]
        if n=="GenImageFontAtlas":
            calls += ['        if(p1 && *p1) vr_handle(*p1,"Rectangle",(size_t)p2,sizeof(Rectangle),vr_memory_drop,0,0);']
        if n == "MemRealloc":
            calls += ["        if(result) vr_invalidate(vr_integer(a[0]));"]
        if n in GPU_OWNERS:
            destructor, kind = GPU_OWNERS[n]
            calls += ["        if(!result) return vr_int(0);", "        unsigned int *copy=vr_alloc(sizeof(result)); *copy=result;"]
            if n=="rlLoadTextureDepth": calls += ['        if(p2) return vr_handle(copy,"GL:Renderbuffer",1,sizeof(result),vr_drop_renderbuffer,1,0);']
            calls += [f'        return vr_handle(copy,"GL:{kind}",1,sizeof(result),vr_gpu_drop_{destructor},1,0);', "    }"]
            continue
        if depth:
            if retbase == "char" and n not in POINTER_OWNERS and depth == 1:
                result = "vr_text(result)"
            elif n=="TextSplit":
                calls += ['        VR texts=vr_list((size_t)*p2,NULL); for(int i=0;i<*p2;++i) texts.items[i]=vr_text(result[i]); return texts;', "    }"]; continue
            else:
                length = "0"; drop = pointer_drop.get(n, "NULL")
                if n in POINTER_OWNERS:
                    rule = POINTER_OWNERS[n][1]
                    argindex = next((i for i, a in enumerate(args) if a["name"] == rule), None)
                    if argindex is not None:
                        length = f"(size_t){'*' if '*' in args[argindex]['type'] else ''}p{argindex}"
                    elif rule == "strlen": length = "result ? strlen(result)+1 : 0"
                    elif rule == "image_pixels": length = "(size_t)p0.width*p0.height"
                    elif rule == "wave_samples": length = "(size_t)p0.frameCount*p0.channels"
                    elif rule == "screen_pixels": length = "(size_t)p0*p1*4"
                    elif rule == "texture_bytes": length = "(size_t)GetPixelDataSize(p1,p2,p3)"
                    else: raise ValueError(f"unknown pointer size {n}: {rule}")
                if n.startswith("Compute") and n in ("ComputeMD5", "ComputeSHA1", "ComputeSHA256"):
                    length = {"ComputeMD5": "4", "ComputeSHA1": "5", "ComputeSHA256": "8"}[n]
                if n == "rlGetShaderLocsDefault": length = "RL_MAX_SHADER_LOCATIONS"
                elem = retbase + "*" * (depth - 1)
                size = "1" if elem == "void" else "sizeof(" + elem + ")"
                result = f'vr_handle((void *)result,"{elem}",{length},{size},{drop},0,0)'
                if n=="LoadMaterials":
                    calls += [f"        VR handle={result}; vr_adopt_materials(handle.integer,result,{length}); return handle;", "    }"]; continue
        elif retbase in RESOURCES:
            drop = "vr_drop_" + DTORS[retbase] if retbase in DTORS and n.startswith(("Load", "Gen", "ImageCopy", "ImageFrom", "ImageText", "WaveCopy", "rlLoadRenderBatch", "GetClipboardImage", "GetFontDefault")) else "NULL"
            if n == "GetFontDefault": drop = "NULL"
            if n == "LoadSoundAlias": drop = "vr_drop_UnloadSoundAlias"
            # Default font and getters borrow native storage; only the struct copy is owned.
            owner = "vr_integer(a[0])" if drop=="NULL" and args and normalize(args[0]["type"])[0] in RESOURCES else "0"
            if n=="GetFontDefault":
                calls += ['        for(size_t i=0;i<vr_capacity;++i) if(vr_slots[i].data && !strcmp(vr_slots[i].type,"Font") && !vr_slots[i].drop && ((Font *)vr_slots[i].data)->texture.id==result.texture.id) return vr_int(vr_id(i));']
            calls += [f"        {retbase} *copy=vr_alloc(sizeof(result)); *copy=result;"]
            result = f'vr_handle(copy,"{retbase}",1,sizeof(result),{drop},1,{owner})'
            if n == "LoadModelFromMesh":
                calls += [f"        VR handle={result}; vr_slot(vr_integer(a[0]))->owner=handle.integer; vr_slot(vr_integer(a[0]))->drop=NULL; return handle;", "    }"]; continue
            if n=="LoadModel":
                calls += [f"        VR handle={result}; vr_adopt_model(handle.integer); return handle;", "    }"]; continue
            if n=="LoadSoundAlias":
                calls += [f"        VR handle={result}; vr_slot(handle.integer)->owner=vr_integer(a[0]); return handle;", "    }"]; continue
            if n=="LoadMusicStreamFromMemory":
                calls += [f"        VR handle={result}; vr_depend(handle.integer,vr_integer(a[1]),0); return handle;", "    }"]; continue
        else: result = pack(fn["return"], "result")
        calls += [f"        return {result};", "    }"]
    # Constants are getters, evaluated by upstream C rather than a second enum evaluator.
    for name in re.findall(r"^#define\s+(\w+)\s+CLITERAL\(Color\)", (ROOT / "vendor/raylib/src/raylib.h").read_text(), re.M):
        constants.append(name)
    constants += ["PI", "DEG2RAD", "RAD2DEG", "EPSILON"]
    colors = set(re.findall(r"^#define\s+(\w+)\s+CLITERAL\(Color\)", (ROOT / "vendor/raylib/src/raylib.h").read_text(), re.M))
    for offset, name in enumerate(dict.fromkeys(constants)):
        ident = len(functions) + offset
        binding(name, ident, 0, property=True)
        result = f"vr_pack_Color({name},0)" if name in colors else f"vr_real({name})" if name in ("PI", "DEG2RAD", "RAD2DEG", "EPSILON") else f"vr_int({name})"
        calls += [f"    case {ident}: return {result};"]
    calls += ['    default: vr_fail("unknown API function"); return vr_null();', "    }", "}"]
    availability = ["static inline const char *vraylibUnavailable(const std::string& name) {"]
    for f in inventory:
        if f["reason"]: availability += [f'    if(name=="{f["vyne"]}") return "{f["reason"]}";']
    availability += ["    return nullptr;", "}"]
    generated = {
        OUT / "generated_types.inc": "\n".join(types) + "\n",
        OUT / "generated_calls.inc": "\n".join(calls) + "\n",
        OUT / "generated_runtime.inc": "// Generated; do not edit.\n" + "\n".join(runtime) + "\n",
        OUT / "api.json": json.dumps({"version": "6.2", "functions": inventory}, indent=2) + "\n",
        ROOT / "vyne/compiler/codegen/vraylib_map.h": "// Generated; do not edit.\n#pragma once\nstatic const NativeMapEntry VRAYLIB_MAP[] = {\n" + "\n".join(maps) + "\n};\n" + "\n".join(availability) + "\nstatic const char *VRAYLIB_FIELDS[] = {" + ",".join(json.dumps(f) for f in sorted({vyfield(f['name']) for fields in structs.values() for f in fields})) + "};\n",
    }
    # Value interfaces let returned records participate in ordinary Vyne member access.
    vytypes = ["# Generated value types; resource types use Int64 handles.", "ruleset { dynamic_casting };", "module vraylib;"]
    for name, fields in structs.items():
        if name in RESOURCES or any(normalize(f["type"])[1] for f in fields): continue
        vytypes += [f"interface {name} {{"]
        for f in fields:
            b, _ = normalize(f["type"])
            t = "vraylib." + b if b in structs and b not in RESOURCES else "Float64" if b in ("float", "double") else "Bool" if b == "bool" else "Int64"
            if f["array"]: t = "Array" if b in structs else f"Array<{t}>"
            vytypes += [f"    {vyfield(f['name'])} :: {t},"]
        vytypes += ["}"]
    generated[ROOT / "vyne/modules/external/vraylib/Types.vy"] = "\n".join(vytypes) + "\n"
    for path, content in generated.items():
        if opts.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                raise SystemExit(f"Generated file is stale: {path.relative_to(ROOT)}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                path.write_text(content, encoding="utf-8", newline="\n")
    print("API inventory:", ", ".join(f"{s}: {sum(f['source']==s for f in inventory)}" for s in headers),
          f"({sum(f['status']=='unavailable' for f in inventory)} explicitly unavailable)")


if __name__ == "__main__":
    main()
