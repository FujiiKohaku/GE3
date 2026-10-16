#include "App/Game/Stage/NormalEnemyLevelTable.h"
#include "Engine/LevelEditor/LevelDataLoader.h"
#include <cassert>
#include <filesystem>
#include <iostream>

namespace {
void WriteFixture(const std::string& path, const nlohmann::json& data)
{
    std::ofstream file(path);
    assert(file);
    file << data.dump(2);
}

nlohmann::json ReadProfiles()
{
    std::ifstream file("project/resources/Enemies/normal-enemy-levels.json");
    assert(file);
    return nlohmann::json::parse(file);
}

void AssertLegacy(const NormalEnemySettings& settings)
{
    assert(settings.hp == 4.0f);
    assert(settings.fireIntervalFrames == 120);
    assert(settings.bulletSpeedPerFrame == 0.48f);
    assert(settings.isRadialShotEnabled);
}

void TestProfilesAndReload()
{
    NormalEnemyLevelTable table;
    assert(table.Load("project/resources/Enemies/normal-enemy-levels.json"));
    const NormalEnemySettings easy = table.GetSettings(1);
    assert(easy.hp == 1.0f && easy.fireIntervalFrames == 180);
    assert(easy.bulletSpeedPerFrame == 0.36f && !easy.isRadialShotEnabled);
    AssertLegacy(table.GetSettings(2));
    assert(table.GetSettings(3).hp == 8.0f && table.GetSettings(3).fireIntervalFrames == 90);
    AssertLegacy(table.GetSettings(0));
    AssertLegacy(table.GetSettings(4));

    const std::string fixturePath = "generated/normal-enemy-level-profiles.json";
    nlohmann::json data = ReadProfiles();
    data["levels"]["1"]["hp"] = 2.0f;
    WriteFixture(fixturePath, data);
    assert(table.Load(fixturePath));
    assert(table.GetSettings(1).hp == 2.0f);
    data["levels"]["1"]["hp"] = 3.0f;
    WriteFixture(fixturePath, data);
    assert(table.Load(fixturePath));
    assert(table.GetSettings(1).hp == 3.0f);
    assert(!table.Load("generated/missing-normal-enemy-levels.json"));
    AssertLegacy(table.GetSettings(1));
    AssertLegacy(table.GetSettings(3));
}

void TestInvalidProfilesFallBackAtomically()
{
    const std::string fixturePath = "generated/normal-enemy-level-invalid.json";
    const std::array<nlohmann::json, 6> invalidValues { 0, -1, "invalid", true, nullptr, 1.0e100 };
    for (const std::string key : { "hp", "fireIntervalFrames", "bulletSpeedPerFrame" }) {
        for (const nlohmann::json& invalidValue : invalidValues) {
            nlohmann::json data = ReadProfiles();
            data["levels"]["1"][key] = invalidValue;
            WriteFixture(fixturePath, data);
            NormalEnemyLevelTable table;
            assert(!table.Load(fixturePath));
            AssertLegacy(table.GetSettings(1));
            assert(table.GetSettings(3).hp == 8.0f);
        }
    }
    nlohmann::json data = ReadProfiles();
    data["levels"]["1"]["fireIntervalFrames"] = 60.5;
    data["levels"]["3"]["isRadialShotEnabled"] = "false";
    WriteFixture(fixturePath, data);
    NormalEnemyLevelTable table;
    assert(!table.Load(fixturePath));
    AssertLegacy(table.GetSettings(1));
    AssertLegacy(table.GetSettings(3));
    {
        std::ofstream file(fixturePath);
        file << "{broken-json";
    }
    assert(!table.Load(fixturePath));
    AssertLegacy(table.GetSettings(2));
}

void TestPlacementLevelsAndCompatibility()
{
    nlohmann::json data;
    data["scene"] = nlohmann::json::array();
    const std::array<nlohmann::json, 10> levels { 1, 2, 3, 0, 4, -1, 1.5, "1", true, 1.0e100 };
    for (const nlohmann::json& level : levels) {
        data["scene"].push_back({ { "name", "NormalFixture" }, { "type", "EnemySpawn" },
            { "file_name", "NormalEnemy" }, { "level", level } });
    }
    data["scene"].push_back({ { "name", "Legacy" }, { "type", "EnemySpawn" }, { "file_name", "NormalEnemy" } });
    data["scene"].push_back({ { "name", "Disabled" }, { "type", "EnemySpawn" },
        { "file_name", "NormalEnemy" }, { "level", 1 }, { "disabled", true } });
    const std::string fixturePath = "generated/normal-enemy-level-placement.json";
    WriteFixture(fixturePath, data);
    LevelDataLoader loader;
    const LevelData levelData = loader.Load(fixturePath);
    assert(levelData.enemies.size() == 11 && levelData.objects.size() == 11);
    for (size_t index = 0; index < 11; ++index) {
        int expectedLevel = 2;
        if (index < 3) {
            expectedLevel = static_cast<int>(index + 1);
        }
        assert(levelData.enemies[index].enemyLevel == expectedLevel);
        assert(levelData.objects[index].enemyLevel == expectedLevel);
    }
    data["scene"][0]["level"] = 3;
    WriteFixture(fixturePath, data);
    assert(loader.Load(fixturePath).objects[0].enemyLevel == 3);
    const LevelData existing = loader.Load("project/resources/Stages/Stage03/layout.json");
    for (const LevelData::EnemySpawnData& enemy : existing.enemies) {
        assert(enemy.enemyLevel == 2);
    }
}
}

int main()
{
    std::filesystem::create_directories("generated");
    TestProfilesAndReload();
    TestInvalidProfilesFallBackAtomically();
    TestPlacementLevelsAndCompatibility();
    std::cout << "NormalEnemyLevelTests passed: profiles, reload, invalid values, legacy placement\n";
}
