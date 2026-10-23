// ステージ・敵・弾・トリガーの衝突を調べ、得点や被弾演出に必要な結果を呼び出し元へ返す。
#pragma once

#include "Engine/Math/MathStruct.h"
#include "Engine/CollisionManager/CollisionManager.h"

#include <memory>
#include <cstdint>
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

// 衝突によって発生した通知。得点や画面演出への反映はシーン側で行う。
struct GameplayCollisionEvents {
    bool paintBulletHitPlayer = false;
    uint32_t scoreAwarded = 0;
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
    // プレイヤーとステージ障害物の接触に対応する。
    void UpdateStageCollisions(
        Player& player,
        const std::vector<std::unique_ptr<Object3d>>& levelObjects);

    // 弾ごとに敵・破壊可能物・床の接触時刻を比較し、最初の命中だけを処理する。
    GameplayCollisionEvents UpdatePlayerBulletCollisions(
        Player& player,
        const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        BaseEnemy* boss,
        std::vector<DestructibleLevelObject>& destructibles,
        Object3d* floorObject);

    // 敵本体と敵弾のプレイヤーへの接触を調べる。
    GameplayCollisionEvents UpdateCombatCollisions(
        Player& player,
        std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        const std::vector<std::unique_ptr<EnemyBullet>>& independentBullets);

    // ステージ内のトリガーとの接触を調べ、ゲーム進行に使うイベントを返す。
    GameplayCollisionEvents UpdateTriggers(
        Player& player,
        std::vector<StageTrigger>& triggers);

    // 照準が参照するレイ判定の対象を、現在の障害物・敵・ボスの配置で登録し直す。
    void SyncRaycastTargets(
        const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        const BaseEnemy* boss);

private:
    friend bool RunGameplayCollisionTests();

    struct PlayerBulletHit {
        SweepHit hit {};
        BaseEnemy* enemy = nullptr;
        DestructibleLevelObject* destructible = nullptr;
        int32_t partIndex = 0;
    };

    static PlayerBulletHit FindFirstPlayerBulletHit(
        const Sphere& bullet, const Vector3& movement,
        const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        BaseEnemy* boss, std::vector<DestructibleLevelObject>& destructibles,
        float floorHeight);
};
