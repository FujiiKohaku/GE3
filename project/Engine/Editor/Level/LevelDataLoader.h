#pragma once
#include "externals/json.hpp"
#include "LevelData.h"
#include <string>

class LevelDataLoader {
public:
    LevelData Load(const std::string& filePath);

private:
    void LoadObject(const nlohmann::json& objectJson, LevelData& levelData);
};