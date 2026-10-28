#include "App/Game/Enemy/LoopEnemy/LoopEnemyPath.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>

namespace {
bool IsClose(float actual, float expected, float tolerance = 0.001f)
{
    return std::abs(actual - expected) <= tolerance;
}

bool TestRouteShape()
{
    const LoopEnemyPathSample entry = LoopEnemyPath::Sample(0.0f);
    const LoopEnemyPathSample junction = LoopEnemyPath::Sample(2.0f);
    const LoopEnemyPathSample left = LoopEnemyPath::Sample(3.0f);
    const LoopEnemyPathSample top = LoopEnemyPath::Sample(4.0f);
    const LoopEnemyPathSample right = LoopEnemyPath::Sample(5.0f);
    const LoopEnemyPathSample completedLoop = LoopEnemyPath::Sample(6.0f);
    const LoopEnemyPathSample exit = LoopEnemyPath::Sample(8.0f);
    return entry.position.x > 50.0f && entry.velocity.x < 0.0f && !entry.hasFinished &&
        IsClose(junction.position.x, 0.0f) && IsClose(junction.position.y, 0.0f) &&
        left.position.x < -15.0f && right.position.x > 15.0f &&
        IsClose(top.position.x, 0.0f) && top.position.y > 27.0f &&
        IsClose(completedLoop.position.x, junction.position.x) && IsClose(completedLoop.position.y, junction.position.y) &&
        exit.position.x < -60.0f && exit.hasFinished;
}

bool TestSingleLoopAndBounds()
{
    float sweptAngleRadians = 0.0f;
    LoopEnemyPathSample previous = LoopEnemyPath::Sample(2.0f);
    for (int index = 1; index <= 400; ++index) {
        const LoopEnemyPathSample sample = LoopEnemyPath::Sample(2.0f + index * 0.01f);
        const float horizontal = sample.position.x / LoopEnemyPath::kHorizontalRadius;
        const float vertical = (sample.position.y - LoopEnemyPath::kVerticalRadius) / LoopEnemyPath::kVerticalRadius;
        if (!IsClose(horizontal * horizontal + vertical * vertical, 1.0f)) { return false; }
        const float previousHorizontal = previous.position.x / LoopEnemyPath::kHorizontalRadius;
        const float previousVertical = (previous.position.y - LoopEnemyPath::kVerticalRadius) / LoopEnemyPath::kVerticalRadius;
        sweptAngleRadians += std::atan2(previousHorizontal * vertical - previousVertical * horizontal,
            previousHorizontal * horizontal + previousVertical * vertical);
        previous = sample;
    }
    if (!IsClose(std::abs(sweptAngleRadians), 2.0f * std::numbers::pi_v<float>, 0.01f)) { return false; }
    for (int index = 0; index <= 800; ++index) {
        const LoopEnemyPathSample sample = LoopEnemyPath::Sample(index * 0.01f);
        if (!std::isfinite(sample.position.x) || !std::isfinite(sample.position.y) ||
            sample.position.x < -64.001f || sample.position.x > 60.001f ||
            sample.position.y < -3.001f || sample.position.y > 28.001f) { return false; }
        const float speedSquared = sample.velocity.x * sample.velocity.x + sample.velocity.y * sample.velocity.y;
        if (!std::isfinite(speedSquared) || speedSquared <= 1.0f) { return false; }
    }
    return true;
}

bool TestSmoothTransitions()
{
    for (float junctionSeconds : std::array<float, 2> { 2.0f, 6.0f }) {
        const LoopEnemyPathSample before = LoopEnemyPath::Sample(junctionSeconds - 0.0001f);
        const LoopEnemyPathSample after = LoopEnemyPath::Sample(junctionSeconds + 0.0001f);
        if (!IsClose(before.position.x, after.position.x, 0.01f) ||
            !IsClose(before.position.y, after.position.y, 0.01f) ||
            !IsClose(before.velocity.x, after.velocity.x, 0.01f) ||
            !IsClose(before.velocity.y, after.velocity.y, 0.01f)) { return false; }
    }
    return true;
}

bool TestSpawnSettingsLoading()
{
    const nlohmann::json sceneJson = nlohmann::json::parse(R"({"scene":[
        {"type":"EnemySpawn","file_name":"LoopEnemy"},
        {"type":"EnemySpawn","file_name":"LoopEnemy","loopEnemy":{"count":3,"entrySide":"left","spawnIntervalSeconds":0.75,"activationDistance":140}},
        {"type":"EnemySpawn","file_name":"LoopEnemy","loopEnemy":{"count":16,"entrySide":"right","spawnIntervalSeconds":0,"activationDistance":40}},
        {"type":"EnemySpawn","file_name":"LoopEnemy","loopEnemy":{"count":0,"entrySide":"bad","spawnIntervalSeconds":-1,"activationDistance":1000}},
        {"type":"EnemySpawn","file_name":"LoopEnemy","loopEnemy":{"count":9999999999999999999,"entrySide":5,"spawnIntervalSeconds":"bad","activationDistance":null}},
        {"type":"EnemySpawn","file_name":"LoopEnemy","loopEnemy":[]},
        {"type":"EnemySpawn","file_name":"NormalEnemy","loopEnemy":{"count":3,"entrySide":"left"}},
        {"type":"EnemySpawn","file_name":"LoopEnemy","loopEnemy":{"count":2.5,"spawnIntervalSeconds":11,"activationDistance":39}}
    ]})");
    const std::filesystem::path testPath = std::filesystem::temp_directory_path() / "KohakuLoopEnemySettingsTests.json";
    {
        std::ofstream testFile(testPath);
        testFile << sceneJson.dump();
    }
    LevelDataLoader loader;
    const LevelData levelData = loader.Load(testPath.string());
    std::filesystem::remove(testPath);
    if (levelData.objects.size() != 8 || levelData.enemies.size() != 8) { return false; }
    for (std::size_t index = 0; index < levelData.objects.size(); ++index) {
        const LevelData::LoopEnemySpawnData& objectSettings = levelData.objects[index].loopEnemy;
        const LevelData::LoopEnemySpawnData& enemySettings = levelData.enemies[index].loopEnemy;
        if (objectSettings.count != enemySettings.count || objectSettings.isEnteringFromLeft != enemySettings.isEnteringFromLeft ||
            !IsClose(objectSettings.spawnIntervalSeconds, enemySettings.spawnIntervalSeconds) ||
            !IsClose(objectSettings.activationDistance, enemySettings.activationDistance)) { return false; }
        if (index == 1) {
            if (objectSettings.count != 3 || !objectSettings.isEnteringFromLeft ||
                !IsClose(objectSettings.spawnIntervalSeconds, 0.75f) || !IsClose(objectSettings.activationDistance, 140.0f)) { return false; }
        } else if (index == 2) {
            if (objectSettings.count != 16 || objectSettings.isEnteringFromLeft ||
                !IsClose(objectSettings.spawnIntervalSeconds, 0.0f) || !IsClose(objectSettings.activationDistance, 40.0f)) { return false; }
        } else {
            if (objectSettings.count != 1 || objectSettings.isEnteringFromLeft ||
                !IsClose(objectSettings.spawnIntervalSeconds, 0.6f) || !IsClose(objectSettings.activationDistance, 120.0f)) { return false; }
        }
    }
    return true;
}

bool TestMirroredPath()
{
    for (int frameIndex = 0; frameIndex <= 960; ++frameIndex) {
        const float elapsedSeconds = static_cast<float>(frameIndex) / 120.0f;
        const LoopEnemyPathSample rightEntry = LoopEnemyPath::Sample(elapsedSeconds);
        const LoopEnemyPathSample leftEntry = LoopEnemyPath::Sample(elapsedSeconds, true);
        if (!IsClose(leftEntry.position.x, -rightEntry.position.x) ||
            !IsClose(leftEntry.position.y, rightEntry.position.y) ||
            !IsClose(leftEntry.velocity.x, -rightEntry.velocity.x) ||
            !IsClose(leftEntry.velocity.y, rightEntry.velocity.y) ||
            leftEntry.hasFinished != rightEntry.hasFinished) { return false; }
    }
    return true;
}

bool TestFrameRateIndependenceAndInvalidTime()
{
    const LoopEnemyPathSample expected = LoopEnemyPath::Sample(4.5f);
    for (int framesPerSecond : std::array<int, 3> { 30, 60, 120 }) {
        float elapsedSeconds = 0.0f;
        const int frameCount = framesPerSecond * 9 / 2;
        for (int frame = 0; frame < frameCount; ++frame) { elapsedSeconds += 1.0f / framesPerSecond; }
        const LoopEnemyPathSample actual = LoopEnemyPath::Sample(elapsedSeconds);
        if (!IsClose(actual.position.x, expected.position.x, 0.002f) ||
            !IsClose(actual.position.y, expected.position.y, 0.002f)) { return false; }
    }
    const LoopEnemyPathSample negative = LoopEnemyPath::Sample(-1.0f);
    const LoopEnemyPathSample invalid = LoopEnemyPath::Sample(std::numeric_limits<float>::quiet_NaN());
    const LoopEnemyPathSample finished = LoopEnemyPath::Sample(100.0f);
    return IsClose(negative.position.x, 60.0f) && IsClose(invalid.position.x, 60.0f) &&
        !invalid.hasFinished && finished.hasFinished && IsClose(finished.position.x, -64.0f);
}
}

int main()
{
    if (!TestRouteShape() || !TestSingleLoopAndBounds() || !TestSmoothTransitions() ||
        !TestFrameRateIndependenceAndInvalidTime() || !TestMirroredPath() || !TestSpawnSettingsLoading()) {
        std::puts("LoopEnemyPathTests FAILED");
        return 1;
    }
    std::puts("LoopEnemyPathTests PASSED: route, FPS independence, mirrored entry, JSON defaults, settings, bounds, invalid values");
    return 0;
}
