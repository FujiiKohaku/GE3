#include "App/Game/Enemy/PaceEnemy/PaceEnemyPath.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
bool IsClose(float actual, float expected)
{
    return std::abs(actual - expected) < 0.002f;
}

bool TestFlight()
{
    LevelData::PaceEnemySpawnData settings;
    settings.offsetX = -8.0f;
    settings.offsetY = 4.0f;
    settings.forwardOffset = 90.0f;
    settings.holdDurationSeconds = 5.3f;
    const float exitStartSeconds = settings.entryDurationSeconds + settings.holdDurationSeconds;
    const auto entry = PaceEnemyPath::Sample(0.0f, settings);
    const auto hold = PaceEnemyPath::Sample(settings.entryDurationSeconds, settings);
    const auto peak = PaceEnemyPath::Sample(settings.entryDurationSeconds + 1.0f, settings);
    const auto beforeExit = PaceEnemyPath::Sample(exitStartSeconds - 0.0001f, settings);
    const auto exit = PaceEnemyPath::Sample(exitStartSeconds, settings);
    const auto end = PaceEnemyPath::Sample(exitStartSeconds + settings.exitDurationSeconds, settings);
    if (!IsClose(entry.position.x, 72.0f) || entry.isPacing || entry.hasFinished ||
        !IsClose(hold.position.x, -8.0f) || !hold.isPacing ||
        !IsClose(peak.position.x, 4.0f) || !IsClose(peak.position.y, 4.0f) ||
        !IsClose(peak.position.z, 90.0f) || !IsClose(beforeExit.position.x, exit.position.x) ||
        exit.isPacing || end.isPacing || !end.hasFinished || !IsClose(end.position.x, -88.0f)) { return false; }
    settings.isEnteringFromLeft = true;
    if (!IsClose(PaceEnemyPath::Sample(0.0f, settings).position.x, -88.0f)) { return false; }
    settings.holdDurationSeconds = 0.0f;
    return !PaceEnemyPath::Sample(settings.entryDurationSeconds, settings).isPacing;
}

bool TestTimingAndOffsets()
{
    for (int frameRate : std::array<int, 3> { 30, 60, 120 }) {
        LevelData::PaceEnemySpawnData settings;
        settings.offsetY = -5.0f;
        settings.holdDurationSeconds = 3.0f;
        int pacingFrames = 0;
        for (int frameIndex = 0; frameIndex <= frameRate * 6; ++frameIndex) {
            const float seconds = static_cast<float>(frameIndex) / static_cast<float>(frameRate);
            const auto sample = PaceEnemyPath::Sample(seconds, settings);
            if (!IsClose(sample.position.y, -5.0f) || !IsClose(sample.position.z, 100.0f)) { return false; }
            if (sample.isPacing) {
                ++pacingFrames;
                if (std::abs(sample.position.x) > settings.swayAmplitude + 0.001f) { return false; }
            }
        }
        if (pacingFrames != frameRate * 3 || !PaceEnemyPath::Sample(6.0f, settings).hasFinished) { return false; }
    }
    LevelData::PaceEnemySpawnData settings;
    settings.swayAmplitude = 0.0f;
    settings.offsetX = 7.0f;
    return IsClose(PaceEnemyPath::Sample(3.0f, settings).position.x, 7.0f);
}

bool TestSettingsLoading()
{
    const auto testPath = std::filesystem::temp_directory_path() / "KohakuPaceEnemyTests.json";
    {
        std::ofstream testFile(testPath);
        testFile << R"({"scene":[
          {"type":"EnemySpawn","file_name":"PaceEnemy","transform":{"translation":[1,400,2]},
           "paceEnemy":{"entrySide":"left","holdDurationSeconds":8,"forwardOffset":90,"offsetX":-12,"offsetY":5,"swayAmplitude":0}},
          {"type":"EnemySpawn","file_name":"PaceEnemy","paceEnemy":{"holdDurationSeconds":-1,"offsetX":"bad","forwardOffset":0,"swayPeriodSeconds":0}},
          {"type":"EnemySpawn","file_name":"PaceEnemy","paceEnemy":[]},
          {"type":"EnemySpawn","file_name":"NormalEnemy","paceEnemy":{"holdDurationSeconds":25}}
        ]})";
    }
    LevelDataLoader loader;
    const auto level = loader.Load(testPath.string());
    std::filesystem::remove(testPath);
    if (level.objects.size() != 4 || level.enemies.size() != 4) { return false; }
    for (std::size_t index = 0; index < 4; ++index) {
        const auto& settings = level.objects[index].paceEnemy;
        const auto& enemySettings = level.enemies[index].paceEnemy;
        if (!PaceEnemyPath::IsValidSettings(settings) ||
            !IsClose(settings.holdDurationSeconds, enemySettings.holdDurationSeconds) ||
            !IsClose(settings.offsetX, enemySettings.offsetX) ||
            !IsClose(settings.offsetY, enemySettings.offsetY) ||
            !IsClose(settings.forwardOffset, enemySettings.forwardOffset) ||
            settings.isEnteringFromLeft != enemySettings.isEnteringFromLeft) { return false; }
        if (index == 0) {
            if (!settings.isEnteringFromLeft || !IsClose(settings.holdDurationSeconds, 8.0f) ||
                !IsClose(settings.forwardOffset, 90.0f) || !IsClose(settings.offsetX, -12.0f) ||
                !IsClose(settings.offsetY, 5.0f) || !IsClose(settings.swayAmplitude, 0.0f) ||
                !IsClose(level.objects[index].translation.z, 400.0f)) { return false; }
        } else if (!IsClose(settings.holdDurationSeconds, 6.0f) || !IsClose(settings.forwardOffset, 100.0f)) { return false; }
    }
    LevelData::PaceEnemySpawnData invalidSettings;
    invalidSettings.holdDurationSeconds = std::numeric_limits<float>::quiet_NaN();
    if (PaceEnemyPath::IsValidSettings(invalidSettings)) { return false; }
    invalidSettings = {};
    invalidSettings.entryDurationSeconds = 0.0f;
    return !PaceEnemyPath::IsValidSettings(invalidSettings);
}
}

int main()
{
    if (!TestFlight()) { std::puts("FAIL: flight phases and custom offsets"); return 1; }
    if (!TestTimingAndOffsets()) { std::puts("FAIL: timing across frame rates"); return 1; }
    if (!TestSettingsLoading()) { std::puts("FAIL: settings loading and validation"); return 1; }
    std::puts("PASS: PaceEnemy flight, timing, offsets and settings");
    return 0;
}
