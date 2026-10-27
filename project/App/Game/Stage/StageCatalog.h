// ステージ一覧と各ステージのJSON設定を読み込み、IDで参照できるようにする。
#pragma once

#include "Engine/math/EngineStruct.h"
#include <string>
#include <vector>
#include "Engine/Shadow/ShadowSettings.h"
#include "RailBranchRoute.h"
#include "RailHeightRange.h"

struct StageSettings {
    ShadowSettings shadows;
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
    bool allRangeMode = false;
    float flightAreaRadius = 600.0f;
    float flightMinHeight = 5.0f;
    float flightMaxHeight = 180.0f;
    std::vector<Vector3> railControlPoints;
    RailBranchSettings railBranch;
    RailHeightRangeSettings railHeightRange;
    RailHeightRangeSettings railLowerHeightRange;
    std::vector<RailHeightZoneSettings> railHeightZones;
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
    // アプリ内で共有するステージ一覧を返す。
    static StageCatalog* GetInstance();

    // 一覧JSONから各設定を読み込み、IDの重複や読み込み失敗を検出する。
    bool Load(const std::string& catalogPath =
        "resources/Stages/catalog.json");
    const std::vector<StageSettings>& GetStages() const { return stages_; }
    // ステージIDに一致する設定を探す。見つからない場合はnullptrを返す。
    const StageSettings* Find(const std::string& id) const;
    const std::string& GetLastError() const { return lastError_; }

private:
    // レイアウト・見た目・敵などのステージ固有設定をJSONから取り込む。
    bool LoadStageSettings(
        const std::string& settingsPath,
        StageSettings& settings);

    std::vector<StageSettings> stages_;
    std::string lastError_;
};
