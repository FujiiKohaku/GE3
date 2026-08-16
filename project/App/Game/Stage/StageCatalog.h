#pragma once

#include "Engine/math/EngineStruct.h"
#include <string>
#include <vector>

struct StageSettings {
    std::string id;
    std::string name;
    std::string description;
    std::string settingsFile;
    std::string layoutFile;
    std::string skybox = "resources/Textures/skybox.dds";
    std::string bgm = "resources/Sounds/BGM.wav";
    std::string floorTexture = "resources/Textures/floor_dirt_gemini.jpg";
    bool floorEnabled = true;
    float floorHeight = -30.0f;
    float railLength = 4600.0f;
    float railPointInterval = 50.0f;
    float railSpeed = 0.5f;
    std::vector<Vector3> railControlPoints;
    std::string bossType = "FearWorm";
    float bossSpawnDistance = 1850.0f;
    Vector3 bossPosition = { 0.0f, 2.0f, 1850.0f };
    bool bossRailAutoExtension = false;
    float bossRailExtensionBuffer = 1200.0f;
    std::vector<float> swarmWaveDistances;
    std::vector<Vector3> recoveryItemPositions;
    std::vector<float> recoveryItemDistances;
    std::vector<float> paintEnemyDistances;
};

class StageCatalog {
public:
    static StageCatalog* GetInstance();

    bool Load(const std::string& catalogPath =
        "resources/Stages/catalog.json");
    const std::vector<StageSettings>& GetStages() const { return stages_; }
    const StageSettings* Find(const std::string& id) const;
    const std::string& GetLastError() const { return lastError_; }

private:
    bool LoadStageSettings(
        const std::string& settingsPath,
        StageSettings& settings);

    std::vector<StageSettings> stages_;
    std::string lastError_;
};
