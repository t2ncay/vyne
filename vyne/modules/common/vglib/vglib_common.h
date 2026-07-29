#pragma once

#include "vglib.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

struct PersistentInstance {
    Vector3 position;
    float scale;
};

extern std::map<std::string, std::vector<PersistentInstance>> persistent_groups;

// helper functions for raw Raylib quad rendering
void DrawCubeTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color);
void DrawPlaneTexture(Texture2D texture, Vector3 centerPos, Vector2 size, Color color);
void DrawBillboardVyne(Camera3D camera, Texture2D texture, Vector3 position, float size, Color color);