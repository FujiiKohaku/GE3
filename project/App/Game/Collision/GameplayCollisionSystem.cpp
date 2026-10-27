// ステージ・敵・弾・トリガーの衝突を調べ、得点や被弾演出に必要な結果を呼び出し元へ返す。
#include "App/Game/Collision/GameplayCollisionSystem.h"

#include "App/Game/Player/Player.h"
#include "App/Game/Player/Combat/Bullet/Common/PlayerBullet.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"
#include "Engine/audio/SoundManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/CollisionManager/BoxCollider.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"

#include <cmath>

namespace {
constexpr float kPlayerObstacleRadius = 1.0f;
constexpr float kPlayerEnemyCollisionRadius = 2.0f;

// 敵弾の移動区間とプレイヤーの当たり判定を取り、ダメージと命中演出を処理する。
void CheckEnemyBulletsAgainstPlayer(
    Player& player,
    const std::vector<std::unique_ptr<EnemyBullet>>& bullets,
    GameplayCollisionEvents& events)
{
    const Sphere playerSphere {
        player.GetTranslate(),
        kPlayerEnemyCollisionRadius * 0.5f
    };
    for (const std::unique_ptr<EnemyBullet>& bullet : bullets) {
        if (!bullet->IsAlive()) {
            continue;
        }

        const Sphere bulletSphere {
            bullet->GetPreviousPosition(),
            bullet->GetCollisionRadius() * 0.5f
        };
        const Vector3 movement =
            bullet->GetPosition() - bullet->GetPreviousPosition();
        const bool hitPlayer = CollisionManager::SweepSphere(
            bulletSphere,
            movement,
            playerSphere).isHit;
        if (!hitPlayer) {
            continue;
        }

        if (bullet->OnHitPlayer(player.GetTranslate()) == EnemyBulletHitEffect::Paint) {
            events.paintBulletHitPlayer = true;
        }
        // 無敵などでダメージが無効なら、被弾エフェクトも重ねて出さない。
        if (player.ApplyDamage(bullet->GetDamage())) {
            EffectManager::GetInstance()->PlayEffect(
                "DamageHit",
                player.GetTranslate());
        }
        bullet->SetDead();
    }
}
}

// プレイヤーとステージ障害物の当たり判定を取り、接触への対応を行う。
void GameplayCollisionSystem::UpdateStageCollisions(
    Player& player,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects)
{
    const Vector3 playerPosition = player.GetTranslate();
    const Sphere playerSphere { playerPosition, kPlayerObstacleRadius };

    for (const std::unique_ptr<Object3d>& levelObject : levelObjects) {
      for (size_t colliderIndex = 0; colliderIndex < levelObject->GetColliderCount(); ++colliderIndex) {
        BoxCollider* collider = levelObject->GetCollider(colliderIndex);
        const OBB obstacleBox = CollisionManager::MakeOBB(
            collider->GetCenter(),
            collider->GetSize(),
            collider->GetRotation());
        if (CollisionManager::Intersect(playerSphere, obstacleBox).isHit) {
            if (player.ApplyDamage(levelObject->GetCollisionDamage())) {
                SoundManager::GetInstance()->Play("EnvironmentCollision");
                EffectManager::GetInstance()->PlayEffect(
                    "DamageHit",
                    playerPosition);
            }
            break;
        }
      }
    }

}

GameplayCollisionSystem::PlayerBulletHit GameplayCollisionSystem::FindFirstPlayerBulletHit(
    const Sphere& bullet, const Vector3& movement,
    const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    BaseEnemy* boss, std::vector<DestructibleLevelObject>& destructibles,
    float floorHeight, bool shouldCollideWithFloor)
{
    PlayerBulletHit nearest {};
    for (DestructibleLevelObject& destructible : destructibles) {
        if (destructible.destroyed || destructible.object == nullptr) {
            continue;
        }
        for (size_t colliderIndex = 0; colliderIndex < destructible.object->GetColliderCount(); ++colliderIndex) {
            const BoxCollider* collider = destructible.object->GetCollider(colliderIndex);
            const OBB box = CollisionManager::MakeOBB(
                collider->GetCenter(), collider->GetSize(), collider->GetRotation());
            const SweepHit hit = CollisionManager::SweepSphere(bullet, movement, box);
            if (hit.isHit && (!nearest.hit.isHit || hit.time < nearest.hit.time)) {
                nearest.hit = hit;
                nearest.destructible = &destructible;
            }
        }
    }

    // 床は従来どおり弾の中心で判定し、到達した時刻と位置を求める。
    SweepHit floorHit {};
    if (shouldCollideWithFloor && bullet.center.y <= floorHeight) {
        floorHit.isHit = true;
        floorHit.time = 0.0f;
    } else if (shouldCollideWithFloor && movement.y < 0.0f && bullet.center.y + movement.y <= floorHeight) {
        floorHit.isHit = true;
        floorHit.time = (floorHeight - bullet.center.y) / movement.y;
    }
    if (floorHit.isHit && (!nearest.hit.isHit || floorHit.time < nearest.hit.time)) {
        floorHit.position = bullet.center + movement * floorHit.time;
        floorHit.position.y = floorHeight;
        nearest.hit = floorHit;
        nearest.destructible = nullptr;
    }

    for (const std::unique_ptr<BaseEnemy>& enemy : enemies) {
        if (enemy->IsDead()) {
            continue;
        }
        int32_t partIndex = 0;
        const SweepHit hit = enemy->SweepBullet(bullet, movement, partIndex);
        if (hit.isHit && (!nearest.hit.isHit || hit.time < nearest.hit.time)) {
            nearest.hit = hit;
            nearest.enemy = enemy.get();
            nearest.destructible = nullptr;
            nearest.partIndex = partIndex;
        }
    }
    if (boss != nullptr && !boss->IsDead()) {
        int32_t partIndex = 0;
        const SweepHit hit = boss->SweepBullet(bullet, movement, partIndex);
        if (hit.isHit && (!nearest.hit.isHit || hit.time < nearest.hit.time)) {
            nearest.hit = hit;
            nearest.enemy = boss;
            nearest.destructible = nullptr;
            nearest.partIndex = partIndex;
        }
    }
    return nearest;
}

GameplayCollisionEvents GameplayCollisionSystem::UpdatePlayerBulletCollisions(
    Player& player, const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    BaseEnemy* boss, std::vector<DestructibleLevelObject>& destructibles,
    Object3d* floorObject, bool shouldCollideWithFloor)
{
    GameplayCollisionEvents events {};
    float floorHeight = -30.0f;
    if (floorObject != nullptr) {
        floorHeight = floorObject->GetTranslate().y;
    }
    for (const std::unique_ptr<PlayerBullet>& bullet : player.GetBullets()) {
        if (!bullet->IsAlive()) {
            continue;
        }
        const Sphere sphere { bullet->GetPreviousPosition(), bullet->GetCollisionRadius() };
        const Vector3 movement = bullet->GetPosition() - bullet->GetPreviousPosition();
        const PlayerBulletHit nearest = FindFirstPlayerBulletHit(
            sphere, movement, enemies, boss, destructibles, floorHeight, shouldCollideWithFloor);
        if (!nearest.hit.isHit) {
            continue;
        }
        if (nearest.enemy != nullptr) {
            BaseEnemy& enemy = *nearest.enemy;
            if (enemy.IsCollisionPartDamageable(nearest.partIndex)) {
                bullet->OnHitEnemy(nearest.hit.position);
                SoundManager::GetInstance()->Play("EnemyHit");
            } else {
                SoundManager::GetInstance()->Play("ArmorDeflect");
            }
            enemy.OnBulletHit(nearest.partIndex, static_cast<float>(bullet->GetDamage()), nearest.hit.position);
            if (enemy.IsDead()) {
                if (&enemy == boss) {
                    SoundManager::GetInstance()->Play("BossDestroyed");
                    events.scoreAwarded += 1000u;
                } else {
                    SoundManager::GetInstance()->Play("EnemyDestroyed");
                    events.scoreAwarded += 100u;
                }
            }
        } else {
            EffectManager::GetInstance()->PlayEffect("HitEffect", nearest.hit.position);
            if (nearest.destructible != nullptr) {
                DestructibleLevelObject& destructible = *nearest.destructible;
                destructible.hp -= static_cast<float>(bullet->GetDamage());
                if (destructible.hp <= 0.0f) {
                    destructible.destroyed = true;
                    const Vector3 center = destructible.object->GetCollider()->GetCenter();
                    EffectManager::GetInstance()->PlayEffect("Explosion", center);
                    for (size_t colliderIndex = 0; colliderIndex < destructible.object->GetColliderCount(); ++colliderIndex) {
                        CollisionManager::GetInstance()->UnregisterCollider(destructible.object->GetCollider(colliderIndex));
                    }
                    destructible.object->ClearColliders();
                    destructible.object->SetScale({ 0.0f, 0.0f, 0.0f });
                    destructible.object->Update();
                }
            }
        }
        bullet->SetDead();
    }
    return events;
}

// 敵本体と敵弾のプレイヤーへの接触を調べ、被弾演出の通知を集める。
GameplayCollisionEvents GameplayCollisionSystem::UpdateCombatCollisions(
    Player& player,
    std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    const std::vector<std::unique_ptr<EnemyBullet>>& independentBullets)
{
    GameplayCollisionEvents events {};
    const Sphere playerSphere {
        player.GetTranslate(),
        kPlayerEnemyCollisionRadius * 0.5f
    };

    for (const std::unique_ptr<BaseEnemy>& enemy : enemies) {
        if (!enemy->IsDead()) {
            std::vector<EnemyCollisionPart> collisionParts;
            enemy->GetCollisionParts(collisionParts);
            for (const EnemyCollisionPart& part : collisionParts) {
                if (CollisionManager::Intersect(
                        playerSphere,
                        Sphere { part.position, part.radius }).isHit) {
                    OutputDebugStringA("Player Hit Enemy\n");
                    break;
                }
            }
        }
    }

    CheckEnemyBulletsAgainstPlayer(player, independentBullets, events);
    return events;
}

// ステージ内のトリガーとの接触を調べ、ゲーム進行に使うイベントを返す。
GameplayCollisionEvents GameplayCollisionSystem::UpdateTriggers(
    Player& player,
    std::vector<StageTrigger>& triggers)
{
    GameplayCollisionEvents events {};
    const Sphere playerSphere {
        player.GetTranslate(),
        kPlayerEnemyCollisionRadius * 0.5f
    };
    for (StageTrigger& trigger : triggers) {
        if (trigger.object == nullptr) {
            continue;
        }
        const OBB triggerBox = CollisionManager::MakeOBB(
            trigger.object->GetTranslate() + trigger.centerOffset,
            trigger.size,
            trigger.object->GetRotate());
        const bool isInside =
            CollisionManager::Intersect(playerSphere, triggerBox).isHit;
        if (isInside &&
            (trigger.type == "WIND" || trigger.type == "GRAVITY")) {
            player.ApplyRailAreaForce(trigger.force);
        }
        if (isInside && !trigger.wasInside) {
            events.enteredTriggers.push_back(trigger.name);
        } else if (!isInside && trigger.wasInside) {
            events.exitedTriggers.push_back(trigger.name);
        }
        trigger.wasInside = isInside;
    }
    return events;
}

// 照準が参照するレイ判定の対象を、現在の障害物・敵・ボスの配置で登録し直す。
void GameplayCollisionSystem::SyncRaycastTargets(
    const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    const BaseEnemy* boss)
{
    CollisionManager* collisionManager = CollisionManager::GetInstance();
    collisionManager->ClearRaycastSphereTargets();
    collisionManager->ClearRaycastObbTargets();

    CollisionObjectId nextObjectId = 1;
    for (const std::unique_ptr<BaseEnemy>& enemy : enemies) {
        enemy->RegisterRaycastTargets(nextObjectId);
    }
    if (boss != nullptr) {
        boss->RegisterRaycastTargets(nextObjectId);
    }
}
