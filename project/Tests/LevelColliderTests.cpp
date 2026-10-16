#include "Engine/LevelEditor/LevelDataLoader.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
bool ContainsPoint(const LevelData::ObjectData::ColliderData& collider, const Vector3& point)
{
    return std::abs(point.x - collider.center.x) <= collider.size.x * 0.5f &&
        std::abs(point.y - collider.center.y) <= collider.size.y * 0.5f &&
        std::abs(point.z - collider.center.z) <= collider.size.z * 0.5f;
}

void TestLegacyAndAdditionalColliderAxes()
{
    const std::string fixturePath = "generated/level-collider-test.json";
    {
        std::ofstream fixture(fixturePath);
        fixture << R"({"scene":[{"name":"Fixture","type":"MESH",
            "collider":{"type":"BOX","center":[1,2,3],"size":[4,5,6]},
            "colliders":[{"type":"BOX","center":[7,8,9],"size":[10,11,12]}]}]})";
    }
    LevelDataLoader loader;
    const LevelData level = loader.Load(fixturePath);
    assert(level.objects.size() == 1);
    const LevelData::ObjectData& object = level.objects[0];
    assert(object.collider.exists);
    assert(object.collider.center.x == 1 && object.collider.center.y == 3 && object.collider.center.z == 2);
    assert(object.collider.size.x == 4 && object.collider.size.y == 6 && object.collider.size.z == 5);
    assert(object.colliders.size() == 1);
    const LevelData::ObjectData::ColliderData& box = object.colliders[0];
    assert(box.exists && box.type == "BOX");
    assert(box.center.x == 7 && box.center.y == 9 && box.center.z == 8);
    assert(box.size.x == 10 && box.size.y == 12 && box.size.z == 11);
    std::filesystem::remove(fixturePath);
}

void TestInstalledArchOpening()
{
    LevelDataLoader loader;
    const LevelData level = loader.Load("project/resources/Scenes/stage01.json");
    int mountainCount = 0;
    for (const LevelData::ObjectData& object : level.objects) {
        assert(object.name != "Gate01_FloatingRuins_ground" && object.name != "Gate01_FloatingRuins_masonry");
        if (object.name != "Stage01_MountainRuins") { continue; }
        ++mountainCount;
        assert(object.fileName == "Environment/MountainRuins/rock_mountain_ruins.obj");
        assert(object.translation.x == 0 && object.translation.y == -30 && object.translation.z == 440);
        assert(object.scale.x == 14.0f && object.scale.y == 9.5f && object.scale.z == 9.5f);
        assert(std::abs(object.rotation.y - 3.14159265f) < 0.00001f);
        assert(object.colliders.size() == 32);
        bool hasLeftWall = false;
        bool hasArchTop = false;
        for (const LevelData::ObjectData::ColliderData& box : object.colliders) {
            assert(box.size.x > 0 && box.size.y > 0 && box.size.z > 0);
            for (float positionX : { -16.0f, 0.0f, 16.0f }) {
                for (float height : { -5.0f, 0.0f, 5.0f }) {
                    for (float progress = 370.0f; progress <= 520.0f; progress += 2.0f) {
                        const Vector3 point { positionX, height, progress };
                        const Vector3 center { object.translation.x + box.center.x,
                            object.translation.y + box.center.y, object.translation.z + box.center.z };
                        const float deltaX = (std::max)(0.0f, std::abs(point.x - center.x) - box.size.x * 0.5f);
                        const float deltaY = (std::max)(0.0f, std::abs(point.y - center.y) - box.size.y * 0.5f);
                        const float deltaZ = (std::max)(0.0f, std::abs(point.z - center.z) - box.size.z * 0.5f);
                        assert(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ > 1.5f * 1.5f);
                    }
                }
            }
            hasLeftWall = hasLeftWall || ContainsPoint(box, { -50, 30, 0 });
            hasArchTop = hasArchTop || ContainsPoint(box, { 0, 80, 0 });
        }
        assert(hasLeftWall && hasArchTop);
    }
    assert(mountainCount == 1);
}

void TestSlalomPassage()
{
    LevelDataLoader loader;
    const LevelData level = loader.Load("project/resources/Scenes/stage01.json");
    int wallCount = 0;
    int groundCount = 0;
    for (const LevelData::ObjectData& object : level.objects) {
        if (object.name == "Slalom01_FloatingRuinsGround") {
            ++groundCount;
            assert(object.collider.exists);
            const float rightEdge = object.translation.x + object.collider.center.x + object.collider.size.x * 0.5f;
            assert(rightEdge <= -2.0f);
        }
        if (object.name != "Slalom01_LeftWall") { continue; }
        ++wallCount;
        assert(object.translation.x == -12 && object.translation.z == 760);
        assert(object.colliders.size() == 6);
        bool hasSolidCenter = false;
        for (const LevelData::ObjectData::ColliderData& box : object.colliders) {
            assert(object.translation.x + box.center.x + box.size.x * 0.5f <= -2.0f);
            hasSolidCenter = hasSolidCenter || ContainsPoint(box, { 0, 12, 0 });
        }
        assert(hasSolidCenter);
    }
    assert(wallCount == 1 && groundCount == 1);
}

void TestStage01FlightPassages()
{
    LevelDataLoader loader;
    const LevelData level = loader.Load("project/resources/Scenes/stage01.json");
    const Vector3 passagePoints[] = {
        { 0, 0, 520 }, { -4, 0, 900 }, { 0, 0, 1120 },
        { -1, 0, 1380 }, { 0, 0, 1640 }, { 0, 0, 2680 },
        { 4, 0, 3100 }, { -4, 0, 3290 }, { 0, 0, 3700 }
    };
    int wallCount = 0;
    int gateCount = 0;
    for (const LevelData::ObjectData& object : level.objects) {
        if (object.fileName == "Environment/Block/block.obj") {
            assert(object.name.starts_with("StonePillar_") || object.name.starts_with("CollapsedPillar_") ||
                (object.hazard.exists && object.hazard.type == "LASER"));
        }
        if (!object.fileName.starts_with("Environment/Ocean/FloatingRuins/")) { continue; }
        if (object.fileName.ends_with("floating_slalom_wall_masonry.obj")) { ++wallCount; }
        if (object.fileName.starts_with("Environment/Ocean/FloatingRuins/floating_gate_")) {
            ++gateCount;
            assert(object.colliders.size() == 3);
            assert(object.scale.x == 1 && object.scale.y == 1 && object.scale.z == 1);
        }
        std::vector<LevelData::ObjectData::ColliderData> boxes = object.colliders;
        if (object.collider.exists) { boxes.push_back(object.collider); }
        for (const Vector3& point : passagePoints) {
            for (const LevelData::ObjectData::ColliderData& box : boxes) {
                const float centerX = object.translation.x + box.center.x;
                const float centerY = object.translation.y + box.center.y;
                const float centerZ = object.translation.z + box.center.z;
                const float deltaX = point.x - std::clamp(point.x, centerX - box.size.x * 0.5f, centerX + box.size.x * 0.5f);
                const float deltaY = point.y - std::clamp(point.y, centerY - box.size.y * 0.5f, centerY + box.size.y * 0.5f);
                const float deltaZ = point.z - std::clamp(point.z, centerZ - box.size.z * 0.5f, centerZ + box.size.z * 0.5f);
                // The player's obstacle collision sphere has radius one.
                assert(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ > 1.0f);
            }
        }
    }
    assert(wallCount == 8 && gateCount == 4);
}

void TestRotatingHazards()
{
    LevelDataLoader loader;
    const LevelData level = loader.Load("project/resources/Scenes/stage01.json");
    int barCount = 0;
    int hubCount = 0;
    for (const LevelData::ObjectData& object : level.objects) {
        if (!object.fileName.starts_with("Environment/Ocean/RuinsRotor/")) { continue; }
        ++barCount;
        assert(object.hazard.exists && object.hazard.type == "LASER" && object.hazard.damage == 2);
        assert(object.gimmick.exists && object.gimmick.type == "ROTATION");
        assert(object.gimmick.axis.x == 0 && object.gimmick.axis.y == 0 && object.gimmick.axis.z == 1);
        assert(object.collider.exists && object.collider.type == "BOX");
        assert(object.collider.size.y == 2 && object.collider.size.z == 6);
        assert(object.scale.x == object.scale.y && object.scale.y == object.scale.z);
        assert(std::abs(object.scale.x * 46 - object.collider.size.x) < 0.0001f);
        if (object.name.starts_with("RotatingLaser_")) {
            assert(object.translation.z == 2350 && object.collider.size.x == 46);
            assert(std::abs(object.gimmick.speed - 0.9f) < 0.0001f);
        } else {
            assert(object.translation.z == 4620 && object.collider.size.x == 48);
            assert(std::abs(object.gimmick.speed + 1.05f) < 0.0001f);
        }
        if (object.fileName.ends_with("ruins_rotor_hub_bar.obj")) {
            ++hubCount;
            assert(object.colliders.size() == 1);
            const LevelData::ObjectData::ColliderData& hub = object.colliders[0];
            assert(hub.center.x == 0 && hub.center.y == 0 && hub.center.z == 0);
            assert(std::abs(hub.size.x - 7.2f * object.scale.x) < 0.0001f);
            assert(hub.size.x == hub.size.y && hub.size.z == 6);
            assert(object.rotation.z == 0);
        } else {
            assert(object.colliders.empty());
            assert(std::abs(object.rotation.z + 1.5708f) < 0.0001f);
        }
    }
    assert(barCount == 4 && hubCount == 2);
}
}

int main()
{
    TestLegacyAndAdditionalColliderAxes();
    TestInstalledArchOpening();
    TestSlalomPassage();
    TestStage01FlightPassages();
    TestRotatingHazards();
    std::cout << "LevelColliderTests passed: legacy axes, arch opening, nine passages, rotating hazards\n";
}
