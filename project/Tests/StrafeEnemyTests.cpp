#include "App/Game/Enemy/StrafeEnemy/StrafeEnemyPath.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
bool IsClose(float first, float second)
{
    return std::abs(first - second) < 0.001f;
}

bool TestRouteAndShots()
{
    LevelData::StrafeEnemySpawnData settings;
    settings.offsetX = 7.0f;
    settings.offsetY = -5.0f;
    settings.forwardOffset = 125.0f;
    settings.sideDistance = 90.0f;
    auto sample = StrafeEnemyPath::Sample(0.0f, settings);
    if (!IsClose(sample.position.x, 97.0f) || sample.hasFinished) { return false; }
    sample = StrafeEnemyPath::Sample(2.0f, settings);
    if (!IsClose(sample.position.x, 7.0f) || !IsClose(sample.position.y, -5.0f) ||
        !IsClose(sample.position.z, 125.0f)) { return false; }
    sample = StrafeEnemyPath::Sample(4.0f, settings);
    if (!IsClose(sample.position.x, -83.0f) || !sample.hasFinished) { return false; }
    settings.isEnteringFromLeft = true;
    if (!IsClose(StrafeEnemyPath::Sample(0.0f, settings).position.x, -83.0f) ||
        !IsClose(StrafeEnemyPath::Sample(4.0f, settings).position.x, 97.0f)) { return false; }
    return StrafeEnemyPath::GetScheduledShotCount(0.999f, settings) == 0 &&
        StrafeEnemyPath::GetScheduledShotCount(1.0f, settings) == 1 &&
        StrafeEnemyPath::GetScheduledShotCount(2.0f, settings) == 2 &&
        StrafeEnemyPath::GetScheduledShotCount(3.0f, settings) == 3;
}

bool TestFrameRates()
{
    for (int frameRate : { 30, 60, 120 }) {
        for (bool isEnteringFromLeft : { false, true }) {
            LevelData::StrafeEnemySpawnData settings;
            settings.isEnteringFromLeft = isEnteringFromLeft;
            float elapsedSeconds = 0.0f;
            int shotsConsumed = 0;
            int shotsFired = 0;
            float previousX = StrafeEnemyPath::Sample(0.0f, settings).position.x;
            for (int frameIndex = 0; frameIndex < frameRate * 5; ++frameIndex) {
                elapsedSeconds += 1.0f / frameRate;
                const auto sample = StrafeEnemyPath::Sample(elapsedSeconds, settings);
                if (sample.hasFinished) { break; }
                if ((isEnteringFromLeft && sample.position.x <= previousX) ||
                    (!isEnteringFromLeft && sample.position.x >= previousX)) { return false; }
                const int count = StrafeEnemyPath::GetScheduledShotCount(elapsedSeconds, settings);
                if (count > shotsConsumed) { ++shotsFired; shotsConsumed = count; }
                previousX = sample.position.x;
            }
            if (shotsFired != 3 || elapsedSeconds < 3.99f || elapsedSeconds > 4.001f + 1.0f / frameRate) { return false; }
        }
    }
    return true;
}

bool TestSettingsLoading()
{
    const auto testPath = std::filesystem::temp_directory_path() / "KohakuStrafeEnemyTests.json";
    {
        std::ofstream file(testPath);
        file << R"({"scene":[
          {"type":"EnemySpawn","file_name":"StrafeEnemy"},
          {"type":"EnemySpawn","file_name":"StrafeEnemy","strafeEnemy":{"entrySide":"left","durationSeconds":5,"forwardOffset":100,"offsetX":-8,"offsetY":6,"sideDistance":90,"shotCount":4,"fireStartProgress":0.2,"fireEndProgress":0.8}},
          {"type":"EnemySpawn","file_name":"StrafeEnemy","strafeEnemy":{"durationSeconds":0,"offsetX":"bad","entrySide":7,"shotCount":99999999999999,"fireStartProgress":0.8,"fireEndProgress":0.2}},
          {"type":"EnemySpawn","file_name":"StrafeEnemy","strafeEnemy":[]},
          {"type":"EnemySpawn","file_name":"NormalEnemy","strafeEnemy":{"durationSeconds":12}}
        ]})";
    }
    LevelDataLoader loader;
    const auto data = loader.Load(testPath.string());
    std::filesystem::remove(testPath);
    if (data.objects.size() != 5 || data.enemies.size() != 5) { return false; }
    for (std::size_t index = 0; index < 5; ++index) {
        const auto& settings = data.objects[index].strafeEnemy;
        const auto& enemySettings = data.enemies[index].strafeEnemy;
        if (!StrafeEnemyPath::IsValidSettings(settings) || settings.shotCount != enemySettings.shotCount ||
            settings.isEnteringFromLeft != enemySettings.isEnteringFromLeft ||
            !IsClose(settings.durationSeconds, enemySettings.durationSeconds) ||
            !IsClose(settings.offsetY, enemySettings.offsetY)) { return false; }
        if (index == 1) {
            if (!settings.isEnteringFromLeft || !IsClose(settings.durationSeconds, 5.0f) ||
                !IsClose(settings.offsetX, -8.0f) || !IsClose(settings.offsetY, 6.0f) || settings.shotCount != 4) { return false; }
        } else if (!IsClose(settings.durationSeconds, 4.0f) || settings.shotCount != 3) { return false; }
    }
    LevelData::StrafeEnemySpawnData invalid;
    invalid.durationSeconds = std::numeric_limits<float>::quiet_NaN();
    if (StrafeEnemyPath::IsValidSettings(invalid)) { return false; }
    const auto stage = loader.Load("resources/Scenes/stage01.json");
    int strafeCount = 0;
    for (const auto& object : stage.objects) {
        if (object.fileName == "StrafeEnemy") {
            if (!StrafeEnemyPath::IsValidSettings(object.strafeEnemy)) { return false; }
            ++strafeCount;
        }
    }
    return strafeCount == 2;
}
}

int main()
{
    if (!TestRouteAndShots()) { std::puts("FAIL: StrafeEnemy route, offsets, mirroring or shots"); return 1; }
    if (!TestFrameRates()) { std::puts("FAIL: StrafeEnemy FPS independence"); return 1; }
    if (!TestSettingsLoading()) { std::puts("FAIL: StrafeEnemy JSON loading or Stage1 placements"); return 1; }
    std::puts("PASS: StrafeEnemy route, shots, offsets, mirrored entry, FPS and JSON settings");
    return 0;
}
