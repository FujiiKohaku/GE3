#pragma once
#include "Engine/Math/EngineStruct.h"
#include <string>
#include <vector>

struct LevelData {

    struct ObjectData {

        std::string name;
        std::string type;
        std::string fileName;

        Vector3 translation;
        Vector3 rotation;
        Vector3 scale;
    };

    std::vector<ObjectData> objects;
};