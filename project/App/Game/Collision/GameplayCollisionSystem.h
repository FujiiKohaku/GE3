#pragma once

#include "Engine/Math/MathStruct.h"

#include <memory>
#include <string>
#include <vector>

class Object3d;
class Player;
class BaseEnemy;
class EnemyBullet;

struct DestructibleLevelObject {
    Object3d* object = nullptr;
    float hp = 1.0f;
    bool destroyed = false;
};

struct GameplayCollisionEvents {
    bool paintBulletHitPlayer = false;
    bool justDodgedEnemyBullet = false;
    std::vector<std::string> enteredTriggers;
    std::vector<std::string> exitedTriggers;
};

struct StageTrigger {
    Object3d* object = nullptr;
    std::string type;
    std::string name;
    Vector3 centerOffset = { 0.0f, 0.0f, 0.0f };
    Vector3 size = { 1.0f, 1.0f, 1.0f };
    Vector3 force = { 0.0f, 0.0f, 0.0f };
    bool wasInside = false;
};

class GameplayCollisionSystem {
public:
    void UpdateStageCollisions(
        Player& player,
        const std::vector<std::unique_ptr<Object3d>>& levelObjects,
        std::vector<DestructibleLevelObject>& destructibles,
        Object3d* floorObject);

    GameplayCollisionEvents UpdateCombatCollisions(
        Player& player,
        std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        BaseEnemy* boss,
        std::vector<std::unique_ptr<EnemyBullet>>& independentBullets);

    GameplayCollisionEvents UpdateTriggers(
        Player& player,
        std::vector<StageTrigger>& triggers);

    void SyncRaycastTargets(
        const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        const BaseEnemy* boss);
};
