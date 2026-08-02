#include "vglib_common.h"

static inline void DrawFace(
    Vector3 v1, Vector3 v2, Vector3 v3, Vector3 v4,
    Vector3 normal
) {
    rlNormal3f(normal.x, normal.y, normal.z);

    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(v1.x, v1.y, v1.z);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(v2.x, v2.y, v2.z);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(v3.x, v3.y, v3.z);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(v4.x, v4.y, v4.z);
}

void DrawCubeTexture(Texture2D texture, Vector3 position,
                     float width, float height, float length,
                     Color color)
{
    float hw = width  * 0.5f;
    float hh = height * 0.5f;
    float hl = length * 0.5f;

    float x = position.x;
    float y = position.y;
    float z = position.z;

    Vector3 p000 = { x - hw, y - hh, z - hl };
    Vector3 p001 = { x - hw, y - hh, z + hl };
    Vector3 p010 = { x - hw, y + hh, z - hl };
    Vector3 p011 = { x - hw, y + hh, z + hl };
    Vector3 p100 = { x + hw, y - hh, z - hl };
    Vector3 p101 = { x + hw, y - hh, z + hl };
    Vector3 p110 = { x + hw, y + hh, z - hl };
    Vector3 p111 = { x + hw, y + hh, z + hl };

    rlCheckRenderBatchLimit(36);
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);

    rlColor4ub(color.r, color.g, color.b, color.a);

    DrawFace(p001, p101, p111, p011, { 0, 0, 1 });
    DrawFace(p100, p000, p010, p110, { 0, 0, -1 });
    DrawFace(p011, p111, p110, p010, { 0, 1, 0 });
    DrawFace(p000, p100, p101, p001, { 0, -1, 0 });
    DrawFace(p101, p100, p110, p111, { 1, 0, 0 });
    DrawFace(p000, p001, p011, p010, { -1, 0, 0 });

    rlEnd();
    rlSetTexture(0);
}

void DrawPlaneTexture(Texture2D texture, Vector3 centerPos, Vector2 size, Color color) {
    float tileX = size.x / 2.0f; 
    float tileY = size.y / 2.0f;

    rlCheckRenderBatchLimit(4);
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlNormal3f(0.0f, 1.0f, 0.0f);

        rlTexCoord2f(0.0f, 0.0f); rlVertex3f(centerPos.x - size.x/2, centerPos.y, centerPos.z - size.y/2);
        rlTexCoord2f(0.0f, tileY); rlVertex3f(centerPos.x - size.x/2, centerPos.y, centerPos.z + size.y/2);
        rlTexCoord2f(tileX, tileY); rlVertex3f(centerPos.x + size.x/2, centerPos.y, centerPos.z + size.y/2);
        rlTexCoord2f(tileX, 0.0f); rlVertex3f(centerPos.x + size.x/2, centerPos.y, centerPos.z - size.y/2);
    rlEnd();
    rlSetTexture(0);
}

void DrawBillboardVyne(Camera3D camera, Texture2D texture, Vector3 position, float size, Color color) {
    DrawBillboard(camera, texture, position, size, color);
}

namespace VGLibNative {

Value native_draw_line_3d(std::vector<Value>& args) {
    if (args.size() < 7) throw std::runtime_error("line_3d() requires x1, y1, z1, x2, y2, z2, color");

    Vector3 start = { (float)args[0].asFloat(), (float)args[1].asFloat(), (float)args[2].asFloat() };
    Vector3 end   = { (float)args[3].asFloat(), (float)args[4].asFloat(), (float)args[5].asFloat() };
    Color color   = GetColor((uint32_t)args[6].asInt());

    DrawLine3D(start, end, color);
    return Value();
}

Value native_draw_cube(std::vector<Value>& args) {
    if (args.size() < 6) throw std::runtime_error("draw_cube() requires x, y, z, size, rotation, and color");
    
    float x = (float)args[0].asFloat();
    float y = (float)args[1].asFloat();
    float z = (float)args[2].asFloat();
    float size = (float)args[3].asFloat();
    float rotAngle = (float)args[4].asFloat();
    Color color = GetColor((uint32_t)args[5].asInt());

    rlPushMatrix();
        rlTranslatef(x, y, z);
        rlRotatef(rotAngle, 1.0f, 1.0f, 1.0f);
        
        rlEnableDepthTest(); 
        
        DrawCube({0, 0, 0}, size, size, size, color);
        DrawCubeWires({0, 0, 0}, size, size, size, Fade(BLACK, 0.5f));
        
        rlDisableDepthTest();
    rlPopMatrix();

    return Value();
}

Value native_draw_plane(std::vector<Value>& args) {
    Vector3 pos = { (float)args[0].asFloat(), (float)args[1].asFloat(), (float)args[2].asFloat() };
    Vector2 size = { (float)args[3].asFloat(), (float)args[4].asFloat() };
    Color color = GetColor((uint32_t)args[5].asInt());
    
    DrawPlane(pos, size, color);
    return Value();
}

Value native_draw_grid(std::vector<Value>& args) {
    int slices = (args.size() > 0) ? (int)args[0].asInt() : 10;
    float spacing = (args.size() > 1) ? (float)args[1].asFloat() : 1.0f;
    
    DrawGrid(slices, spacing);
    return Value();
}

Value native_load_texture(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("load_texture() requires a file path");
    std::string path = args[0].asString();
    
    Texture2D* tex = new Texture2D(LoadTexture(path.c_str()));
    return Value(reinterpret_cast<int64_t>(tex));
}

Value native_draw_cube_texture(std::vector<Value>& args) {
    if (args.size() < 5) throw std::runtime_error("cube_texture() requires tex_ptr, x, y, z, size");

    Texture2D* tex = reinterpret_cast<Texture2D*>(args[0].asInt());
    Vector3 pos = { (float)args[1].asFloat(), (float)args[2].asFloat(), (float)args[3].asFloat() };
    float size = (float)args[4].asFloat();
    Color color = (args.size() > 5) ? GetColor((uint32_t)args[5].asInt()) : WHITE;

    DrawCubeTexture(*tex, pos, size, size, size, color);
    return Value();
}

Value native_draw_plane_texture(std::vector<Value>& args) {
    Texture2D* tex = reinterpret_cast<Texture2D*>(args[0].asInt());
    Vector3 pos = { (float)args[1].asFloat(), (float)args[2].asFloat(), (float)args[3].asFloat() };
    Vector2 size = { (float)args[4].asFloat(), (float)args[5].asFloat() };
    
    DrawPlaneTexture(*tex, pos, size, WHITE);
    return Value();
}

Value native_draw_billboard(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("draw_billboard() requires camera_ptr, tex_ptr, pos_list, size");

    Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
    Texture2D* tex = reinterpret_cast<Texture2D*>(args[1].asInt());
    
    std::vector<Value> pList = args[2].asList();
    if (pList.size() < 3) throw std::runtime_error("billboard pos_list must have 3 elements [x,y,z]");

    Vector3 pos = { 
        (float)pList[0].asFloat(), 
        (float)pList[1].asFloat(), 
        (float)pList[2].asFloat() 
    };
    
    float size = (float)args[3].asFloat();
    Color color = (args.size() > 4) ? GetColor((uint32_t)args[4].asInt()) : WHITE;

    if (camera && tex) {
        DrawBillboard(*camera, *tex, pos, size, color);
    }
    return Value();
}

Value native_load_model(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("load_model() requires a file path");
    std::string path = args[0].asString();
    
    Model* model = new Model(LoadModel(path.c_str()));
    
    if (model->meshCount == 0) {
        delete model;
        throw std::runtime_error("Model Error: Could not load model from " + path);
    }
    
    return Value(reinterpret_cast<int64_t>(model));
}

Value native_set_model_texture(std::vector<Value>& args) {
    if (args.size() < 2) return Value(false);

    Model* model = reinterpret_cast<Model*>(args[0].asInt());
    Texture2D* tex = reinterpret_cast<Texture2D*>(args[1].asInt());

    if (model != nullptr && tex != nullptr) {
        if (model->materialCount > 0 && model->meshCount > 0) {
            model->materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
            return Value(true);
        }
    }
    std::cout << "Vyne Warning: Model texture could not be set!" << std::endl;
    return Value(false);
}

Value native_draw_model(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("draw_model() requires model_ptr, x, y, z");

    Model* model = reinterpret_cast<Model*>(args[0].asInt());
    Vector3 pos = { (float)args[1].asFloat(), (float)args[2].asFloat(), (float)args[3].asFloat() };
    float scale = (args.size() > 4) ? (float)args[4].asFloat() : 1.0f;
    Color color = (args.size() > 5) ? GetColor((uint32_t)args[5].asInt()) : WHITE;

    if (model) {
        DrawModel(*model, pos, scale, color);
    }
    return Value();
}

Value native_load_map(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("load_map() requires a path");
    std::string path = args[0].asString();
    
    std::ifstream file(path);
    if (!file.is_open()) return Value(std::vector<Value>{});

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    std::vector<Value> final_map;
    std::stringstream ss(content);
    std::string segment;

    while (std::getline(ss, segment, ';')) {
        if (segment.empty() || segment == "\n") continue;
        std::stringstream obj_ss(segment);
        std::string val;
        std::vector<Value> obj_data;
        
        while (std::getline(obj_ss, val, ',')) {
            if (!val.empty()) {
                obj_data.emplace_back(static_cast<double>(std::atof(val.c_str())));
            }
        }
        
        if (obj_data.size() < 5) {
            obj_data.emplace_back(0.0); 
        }
        
        final_map.emplace_back(Value(obj_data));
    }
    
    return Value(final_map);
}

Value native_get_ray_grid(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("get_ray_grid() requires camera_ptr, distance, and grid_size");

    Camera3D* camera = reinterpret_cast<Camera3D*>(args[0].asInt());
    float distance = (float)args[1].asFloat();
    float grid_size = (float)args[2].asFloat();

    if (camera) {
        Vector3 forward = {
            camera->target.x - camera->position.x,
            camera->target.y - camera->position.y,
            camera->target.z - camera->position.z
        };

        float len = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
        if (len > 0) {
            forward.x /= len; forward.y /= len; forward.z /= len;
        }

        Vector3 hitPoint = {
            camera->position.x + forward.x * distance,
            camera->position.y + forward.y * distance,
            camera->position.z + forward.z * distance
        };

        float snappedX = roundf(hitPoint.x / grid_size) * grid_size;
        float snappedY = roundf(hitPoint.y / grid_size) * grid_size;
        float snappedZ = roundf(hitPoint.z / grid_size) * grid_size;

        return Value(std::vector<Value>{ Value(snappedX), Value(snappedY), Value(snappedZ) });
    }
    return Value(std::vector<Value>{Value(0.0), Value(0.0), Value(0.0)});
}

Value native_export_obj(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("export_obj() requires map_data and filename");
    
    std::vector<Value> map_data = args[0].asList();
    std::string filename = args[1].asString();
    
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.is_open()) throw std::runtime_error("Could not create OBJ file");

    file << "# Vyne Static Map Export\n";
    file << "o MapMesh\n";

    file << "vt 0.0 0.0\n";
    file << "vt 1.0 0.0\n";
    file << "vt 1.0 1.0\n";
    file << "vt 0.0 1.0\n";

    int v_count = 1;

    for (const auto& obj_val : map_data) {
        std::vector<Value> obj = obj_val.asList();
        float x = (float)obj[0].asFloat();
        float y = (float)obj[1].asFloat();
        float z = (float)obj[2].asFloat();
        float s = (float)obj[3].asFloat() / 2.0f;

        file << "v " << x-s << " " << y-s << " " << z-s << "\n";
        file << "v " << x+s << " " << y-s << " " << z-s << "\n";
        file << "v " << x+s << " " << y+s << " " << z-s << "\n";
        file << "v " << x-s << " " << y+s << " " << z-s << "\n";
        file << "v " << x-s << " " << y-s << " " << z+s << "\n";
        file << "v " << x+s << " " << y-s << " " << z+s << "\n";
        file << "v " << x+s << " " << y+s << " " << z+s << "\n";
        file << "v " << x-s << " " << y+s << " " << z+s << "\n";

        int o = v_count;
        file << "f " << o << "/1 " << o+1 << "/2 " << o+2 << "/3 " << o+3 << "/4\n";
        file << "f " << o+4 << "/1 " << o+5 << "/2 " << o+6 << "/3 " << o+7 << "/4\n";
        file << "f " << o << "/1 " << o+1 << "/2 " << o+5 << "/3 " << o+4 << "/4\n";
        file << "f " << o+2 << "/1 " << o+3 << "/2 " << o+7 << "/3 " << o+6 << "/4\n";
        file << "f " << o << "/1 " << o+3 << "/2 " << o+7 << "/3 " << o+4 << "/4\n";
        file << "f " << o+1 << "/1 " << o+2 << "/2 " << o+6 << "/3 " << o+5 << "/4\n";

        v_count += 8;
    }

    file << "\n";
    file.close();
    return Value(true);
}

Value native_distance_3d(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("distance_3d() requires two position lists");
    
    std::vector<Value> a = args[0].asList();
    std::vector<Value> b = args[1].asList();
    
    float dx = (float)a[0].asFloat() - (float)b[0].asFloat();
    float dy = (float)a[1].asFloat() - (float)b[1].asFloat();
    float dz = (float)a[2].asFloat() - (float)b[2].asFloat();
    
    return Value((double)sqrtf(dx*dx + dy*dy + dz*dz));
}

Value native_check_collision(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("check_collision() requires player_pos, player_size, cube_pos, cube_size");

    Vector3 pPos = { (float)args[0].asList()[0].asFloat(), (float)args[0].asList()[1].asFloat(), (float)args[0].asList()[2].asFloat() };
    Vector3 pSize = { (float)args[1].asList()[0].asFloat(), (float)args[1].asList()[1].asFloat(), (float)args[1].asList()[2].asFloat() };

    Vector3 cPos = { (float)args[2].asList()[0].asFloat(), (float)args[2].asList()[1].asFloat(), (float)args[2].asList()[2].asFloat() };
    float cSize = (float)args[3].asFloat();

    BoundingBox playerBox = { 
        (Vector3){ pPos.x - pSize.x/2, pPos.y - pSize.y/2 + 0.2f, pPos.z - pSize.z/2 },
        (Vector3){ pPos.x + pSize.x/2, pPos.y + pSize.y/2, pPos.z + pSize.z/2 }
    };

    BoundingBox cubeBox = {
        (Vector3){ cPos.x - cSize/2, cPos.y - cSize/2, cPos.z - cSize/2 },
        (Vector3){ cPos.x + cSize/2, cPos.y + cSize/2, cPos.z + cSize/2 }
    };

    return Value(CheckCollisionBoxes(playerBox, cubeBox));
}

Value native_check_collision_map(std::vector<Value>& args) {
    if (args.size() < 3) throw std::runtime_error("check_collision_map() requires player_pos, player_size, and map_data");

    std::vector<Value> pPosList = args[0].asList();
    std::vector<Value> pSizeList = args[1].asList();
    
    Vector3 pPos = { (float)pPosList[0].asFloat(), (float)pPosList[1].asFloat(), (float)pPosList[2].asFloat() };
    Vector3 pSize = { (float)pSizeList[0].asFloat(), (float)pSizeList[1].asFloat(), (float)pSizeList[2].asFloat() };

    BoundingBox playerBox = { 
        (Vector3){ pPos.x - pSize.x/2, pPos.y - pSize.y/2, pPos.z - pSize.z/2 },
        (Vector3){ pPos.x + pSize.x/2, pPos.y + pSize.y/2, pPos.z + pSize.z/2 }
    };

    std::vector<Value> mapData = args[2].asList();

    for (const auto& objVal : mapData) {
        std::vector<Value> obj = objVal.asList();
        
        float cX = (float)obj[0].asFloat();
        float cY = (float)obj[1].asFloat();
        float cZ = (float)obj[2].asFloat();
        float cSize = (float)obj[3].asFloat();

        BoundingBox cubeBox = {
            (Vector3){ cX - cSize/2, cY - cSize/2, cZ - cSize/2 },
            (Vector3){ cX + cSize/2, cY + cSize/2, cZ + cSize/2 }
        };

        if (CheckCollisionBoxes(playerBox, cubeBox)) {
            return Value(true);
        }
    }

    return Value(false);
}

Value native_pathfind(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("pathfind() requires start_pos, target_pos, map_data, and grid_size");

    std::vector<Value> startList = args[0].asList();
    std::vector<Value> targetList = args[1].asList();
    std::vector<Value> mapData = args[2].asList();
    float gridSize = (float)args[3].asFloat();

    Vector3 start = { (float)startList[0].asFloat(), (float)startList[1].asFloat(), (float)startList[2].asFloat() };
    Vector3 target = { (float)targetList[0].asFloat(), (float)targetList[1].asFloat(), (float)targetList[2].asFloat() };

    Vector3 bestDir = { target.x - start.x, 0, target.z - start.z };
    float len = sqrtf(bestDir.x * bestDir.x + bestDir.z * bestDir.z);
    
    if (len > 0.1f) {
        bestDir.x /= len;
        bestDir.z /= len;
    }

    Vector3 testPos = { start.x + bestDir.x * gridSize, start.y, start.z + bestDir.z * gridSize };
    
    BoundingBox nextBotBox = {
        (Vector3){ testPos.x - 1.0f, testPos.y - 0.5f, testPos.z - 1.0f },
        (Vector3){ testPos.x + 1.0f, testPos.y + 1.5f, testPos.z + 1.0f }
    };

    bool collision = false;
    for (const auto& objVal : mapData) {
        std::vector<Value> obj = objVal.asList();
        float s = (float)obj[3].asFloat();
        BoundingBox wallBox = {
            (Vector3){ (float)obj[0].asFloat() - s/2, (float)obj[1].asFloat() - s/2, (float)obj[2].asFloat() - s/2 },
            (Vector3){ (float)obj[0].asFloat() + s/2, (float)obj[1].asFloat() + s/2, (float)obj[2].asFloat() + s/2 }
        };

        if (CheckCollisionBoxes(nextBotBox, wallBox)) {
            collision = true;
            break;
        }
    }

    if (collision) {
        float angle = 0.785f; // 45 degrees
        Vector3 altDir = {
            bestDir.x * cosf(angle) - bestDir.z * sinf(angle),
            0,
            bestDir.x * sinf(angle) + bestDir.z * cosf(angle)
        };
        bestDir = altDir;
    }

    return Value(std::vector<Value>{ Value(bestDir.x), Value(bestDir.y), Value(bestDir.z) });
}

Value native_set_model_mesh_texture(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("set_model_texture_all() requires model_ptr and tex_ptr");

    Model* model = reinterpret_cast<Model*>(args[0].asInt());
    Texture2D* tex = reinterpret_cast<Texture2D*>(args[1].asInt());

    if (model && tex) {
        for (int i = 0; i < model->materialCount; i++) {
            model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = *tex;
        }
        return Value(true);
    }
    return Value(false);
}

Value native_set_model_alpha_cutoff(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("set_alpha_cutoff() requires model_ptr and threshold (0.0 - 1.0)");

    Model* model = reinterpret_cast<Model*>(args[0].asInt());
    float cutoff = (float)args[1].asFloat();

    if (model) {
        for (int i = 0; i < model->materialCount; i++) {
            int loc = GetShaderLocation(model->materials[i].shader, "alphaCutoff");
            if (loc != -1) {
                SetShaderValue(model->materials[i].shader, loc, &cutoff, SHADER_UNIFORM_FLOAT);
            }
        }
    }
    return Value();
}

Value native_set_alpha_discard(std::vector<Value>& args) {
    if (args.empty()) throw std::runtime_error("set_alpha_discard() requires model_ptr");

    Model* model = reinterpret_cast<Model*>(args[0].asInt());
    
    if (model) {
        for (int i = 0; i < model->materialCount; i++) {
            SetTextureFilter(model->materials[i].maps[MATERIAL_MAP_DIFFUSE].texture, TEXTURE_FILTER_POINT);
        }
        return Value(true);
    }
    return Value(false);
}

Value native_draw_instances(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("draw_instances() requires model_ptr and instances_list");

    Model* model = reinterpret_cast<Model*>(args[0].asInt());
    std::vector<Value> instances = args[1].asList();

    if (!model) return Value();

    for (const auto& instance_val : instances) {
        std::vector<Value> data = instance_val.asList();
        
        if (data.size() < 4) continue;

        Vector3 pos = { (float)data[0].asFloat(), (float)data[1].asFloat(), (float)data[2].asFloat() };
        float scale = (float)data[3].asFloat();
        Color col = WHITE;
        
        DrawModel(*model, pos, scale, col);
    }
    return Value();
}

Value native_draw_instances_ex(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("draw_instances() requires model_ptr, instances_list, cam_pos, and max_dist");

    Model* model = reinterpret_cast<Model*>(args[0].asInt());
    std::vector<Value> instances = args[1].asList();
    std::vector<Value> camPosList = args[2].asList();
    float maxDistSq = (float)args[3].asFloat() * (float)args[3].asFloat();

    Vector3 camPos = { (float)camPosList[0].asFloat(), (float)camPosList[1].asFloat(), (float)camPosList[2].asFloat() };

    if (!model) return Value();

    for (const auto& instance_val : instances) {
        std::vector<Value> data = instance_val.asList();
        if (data.size() < 4) continue;

        float dx = (float)data[0].asFloat() - camPos.x;
        float dz = (float)data[2].asFloat() - camPos.z;
        float distSq = dx*dx + dz*dz;

        if (distSq < maxDistSq) {
            Vector3 pos = { (float)data[0].asFloat(), (float)data[1].asFloat(), (float)data[2].asFloat() };
            float scale = (float)data[3].asFloat();
            DrawModel(*model, pos, scale, WHITE);
        }
    }
    return Value();
}

Value native_upload_persistent_group(std::vector<Value>& args) {
    if (args.size() < 2) throw std::runtime_error("upload_persistent_group() requires group_name and data_list");
    
    std::string groupName = args[0].asString();
    std::vector<Value> instances = args[1].asList();
    
    auto& group = persistent_groups[groupName];
    group.clear();

    for (const auto& inst_val : instances) {
        std::vector<Value> d = inst_val.asList();
        group.push_back({
            {(float)d[0].asFloat(), (float)d[1].asFloat(), (float)d[2].asFloat()},
            (float)d[3].asFloat()
        });
    }
    return Value(true);
}

Value native_draw_persistent_group(std::vector<Value>& args) {
    if (args.size() < 4) throw std::runtime_error("draw_persistent_group() requires name, model, cam_pos, max_dist");

    std::string groupName = args[0].asString();
    Model* model = reinterpret_cast<Model*>(args[1].asInt());
    std::vector<Value> camPosList = args[2].asList();
    float maxDistSq = (float)args[3].asFloat() * (float)args[3].asFloat();

    Vector3 camPos = {(float)camPosList[0].asFloat(), 0, (float)camPosList[2].asFloat()};

    if (persistent_groups.find(groupName) == persistent_groups.end()) return Value();

    const auto& instances = persistent_groups[groupName];
    for (const auto& inst : instances) {
        float dx = inst.position.x - camPos.x;
        float dz = inst.position.z - camPos.z;
        
        if ((dx*dx + dz*dz) < maxDistSq) {
            DrawModel(*model, inst.position, inst.scale, WHITE);
        }
    }
    return Value();
}

} // namespace VGLibNative