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
struct PlayerBulletExplosionSettings;

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
    // 突進の移動を障害物で止め、射撃判定より先に実際の軌道を確定する。
    void UpdateEnemyMovementCollisions(
        std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        const std::vector<std::unique_ptr<Object3d>>& levelObjects);
    // プレイヤーとステージ障害物の接触に対応する。
    void UpdateStageCollisions(
        Player& player,
        const std::vector<std::unique_ptr<Object3d>>& levelObjects);

    // 弾ごとに敵・障害物・破壊可能物・床の接触時刻を比較し、最初の命中だけを処理する。
    GameplayCollisionEvents UpdatePlayerBulletCollisions(
        Player& player,
        const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        BaseEnemy* boss,
        std::vector<DestructibleLevelObject>& destructibles,
        Object3d* floorObject, bool shouldCollideWithFloor = true,
        const std::vector<std::unique_ptr<Object3d>>& levelObjects = {});

    // 敵本体と敵弾のプレイヤーへの接触を調べ、障害物が手前なら敵弾を遮る。
    GameplayCollisionEvents UpdateCombatCollisions(
        Player& player,
        std::vector<std::unique_ptr<BaseEnemy>>& enemies,
        const std::vector<std::unique_ptr<EnemyBullet>>& independentBullets,
        const std::vector<std::unique_ptr<Object3d>>& levelObjects = {},
        const Vector3* previousPlayerPosition = nullptr);

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
        float floorHeight, bool shouldCollideWithFloor = true,
        const std::vector<std::unique_ptr<Object3d>>& levelObjects = {});
    // 直撃対象を除き、各敵の有効な部位へ最大一回だけ爆風ダメージを与える。
    static bool ApplyExplosionToEnemy(BaseEnemy& enemy, const Sphere& explosion, float damage,
        const BaseEnemy* directEnemy);
    static uint32_t ApplyExplosionDamage(const Vector3& position, const PlayerBulletExplosionSettings& settings,
        const std::vector<std::unique_ptr<BaseEnemy>>& enemies, BaseEnemy* boss, const BaseEnemy* directEnemy);
};
