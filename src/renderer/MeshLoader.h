#pragma once
#include "Mesh.h"      // ← this line must be here
#include <string>

class MeshLoader {
public:
    static Mesh load(const std::string& path);
};