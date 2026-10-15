#include "App/Game/Stage/EnemyAppearanceTable.h"
#include "App/Game/Stage/EnemyAppearanceParts.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main()
{
    const std::array<std::string_view, 5> ids { "NormalEnemy", "MoveEnemy", "ArmoredEnemy", "SwarmEnemy", "PaintShooterEnemy" };
    const std::array<float, 5> scales { 2, 2, 2.6f, 0.58f, 2 };
    for (size_t index = 0; index < ids.size(); ++index) {
        const std::string modelName = "Enemy/Ruins/" + std::string(ids[index]) + ".obj";
        const std::string frozenName = "Enemy/Frozen/" + std::string(ids[index]) + ".obj";
        const std::string bodyName = "Enemy/Ruins/" + std::string(ids[index]) + "_Body.obj";
        assert(FindEnemyAppearance("stage01", ids[index]) == bodyName);
        assert(FindEnemyAppearance("stage03", ids[index]) == frozenName);
        assert(FindEnemyAppearance("tutorial", ids[index]).empty());
        assert(FindEnemyAppearance("stage02", ids[index]).empty());
        const std::filesystem::path modelPath = std::filesystem::path("project/resources/Models") / modelName;
        std::ifstream modelFile(modelPath);
        assert(modelFile.is_open());
        float radius = 0;
        int triangleCount = 0;
        std::string line;
        while (std::getline(modelFile, line)) {
            if (line.starts_with("v ")) {
                std::istringstream vertex(line.substr(2));
                float positionX = 0;
                float positionY = 0;
                float positionZ = 0;
                vertex >> positionX >> positionY >> positionZ;
                radius = (std::max)(radius, std::sqrt(positionX * positionX + positionY * positionY + positionZ * positionZ));
            }
            if (line.starts_with("f ")) { ++triangleCount; }
            if (line.starts_with("mtllib ")) {
                const std::filesystem::path materialPath = modelPath.parent_path() / line.substr(7);
                std::ifstream materialFile(materialPath);
                assert(materialFile.is_open());
                std::string materialLine;
                while (std::getline(materialFile, materialLine)) {
                    if (materialLine.starts_with("map_Kd ")) {
                        assert(std::filesystem::exists(materialPath.parent_path() / materialLine.substr(7)));
                    }
                }
            }
        }
        float collisionRadius = 3;
        if (ids[index] == "SwarmEnemy") {
            collisionRadius = 1.15f;
            assert(triangleCount <= 200);
        }
        assert(radius > 0 && (radius + 0.11f) * scales[index] <= collisionRadius);
        assert(triangleCount > 0 && triangleCount < 1500);
        int splitTriangleCount = 0;
        int objectCount = 1;
        std::vector<std::string> partModels { bodyName };
        for (const EnemyAppearancePartEntry& part : kEnemyAppearanceParts) {
            if (part.appearance == ids[index]) {
                ++objectCount;
                partModels.push_back(std::string(part.model));
            }
        }
        if (ids[index] == "SwarmEnemy") { assert(objectCount == 2); }
        for (const std::string& partName : partModels) {
            std::ifstream partFile(std::filesystem::path("project/resources/Models") / partName);
            assert(partFile.is_open());
            int partTriangles = 0;
            float partRadius = 0.0f;
            while (std::getline(partFile, line)) {
                if (line.starts_with("f ")) { ++partTriangles; }
                if (line.starts_with("v ")) {
                    std::istringstream vertex(line.substr(2));
                    float positionX = 0;
                    float positionY = 0;
                    float positionZ = 0;
                    vertex >> positionX >> positionY >> positionZ;
                    partRadius = (std::max)(partRadius, std::sqrt(positionX * positionX + positionY * positionY + positionZ * positionZ));
                }
            }
            assert(partTriangles > 0);
            splitTriangleCount += partTriangles;
            const float maxTranslate = (std::max)(0.0f, collisionRadius / scales[index] - partRadius - kEnemyAppearanceBodyHover - 0.002f);
            for (const EnemyAppearancePartEntry& entry : kEnemyAppearanceParts) {
                if (entry.model != partName) { continue; }
                for (int frameIndex = 0; frameIndex < 3600; ++frameIndex) {
                    const Vector3 desired = CalculateEnemyPartTranslate(entry.motion, frameIndex / 60.0f,
                        1.7f, { 0.14f, -0.10f, 0 }, 0.16f);
                    const Vector3 limited = LimitEnemyPartTranslate(desired, maxTranslate);
                    const float distance = std::sqrt(limited.x * limited.x + limited.y * limited.y + limited.z * limited.z);
                    assert((partRadius + distance + kEnemyAppearanceBodyHover) * scales[index] <= collisionRadius);
                }
            }
        }
        assert(splitTriangleCount == triangleCount);
    }
    assert(FindEnemyAppearance("stage01", "UnknownEnemy").empty());
    assert(FindEnemyAppearance("stage01", "").empty());
    const std::array<EnemyAppearancePart, 6> motions { EnemyAppearancePart::Eye, EnemyAppearancePart::Left,
        EnemyAppearancePart::Right, EnemyAppearancePart::Tail, EnemyAppearancePart::Cannon, EnemyAppearancePart::Tank };
    for (int frameIndex = 0; frameIndex < 3600; ++frameIndex) {
        for (EnemyAppearancePart motion : motions) {
            const Vector3 offset = CalculateEnemyPartTranslate(motion, frameIndex / 60.0f, 1.7f, { 0.14f, -0.10f, 0 }, 0.16f);
            const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
            assert(std::isfinite(distance) && distance + kEnemyAppearanceBodyHover <= 0.22f);
        }
    }
    assert(CalculateEnemyPartTranslate(EnemyAppearancePart::Cannon, 0, 0, {}, 0).z == 0);
    assert(CalculateEnemyPartTranslate(EnemyAppearancePart::Cannon, 0, 0, {}, 0.16f).z > 0);
    const Vector3 travel { 0.14f, 0.10f, 0 };
    const Vector3 gaze = CalculateEnemyEyeTranslate({ 10, -5, -100 }, travel);
    assert(gaze.x > 0.10f && gaze.y < 0);
    const Vector3 oppositeGaze = CalculateEnemyEyeTranslate({ -10, 5, -100 }, travel);
    assert(oppositeGaze.x < 0 && oppositeGaze.y > 0);
    const Vector3 centeredGaze = CalculateEnemyEyeTranslate({ 0, 0, -100 }, travel);
    assert(centeredGaze.x == 0 && centeredGaze.y == 0);
    const Vector3 limitedGaze = CalculateEnemyEyeTranslate({ 1000, -1000, 0 }, travel);
    assert(limitedGaze.x == travel.x && limitedGaze.y == -travel.y);
    std::cout << "RuinsEnemyAssetTests passed: split parts, stage isolation, motion bounds, recoil\n";
}
