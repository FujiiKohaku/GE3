#include "App/Game/Collision/GameplayCollisionSystem.h"

#include "App/Game/Player/Player.h"
#include "App/Game/Player/Bullet/PlayerBullet.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "App/Game/Enemy/Bullet/EnemyBullet.h"
#include "Engine/audio/SoundManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/CollisionManager/BoxCollider.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"

#include <cmath>

namespace {
constexpr float kPlayerObstacleRadius = 1.0f;
constexpr float kPlayerEnemyCollisionRadius = 2.0f;

bool EnemyBlocksBulletHit(const BaseEnemy* enemy, const Sphere& bullet,
    const Vector3& movement, float targetHitTime)
{
    if (enemy == nullptr || enemy->IsDead()) {
        return false;
    }
    std::vector<EnemyCollisionPart> parts;
    enemy->GetCollisionParts(parts);
    for (const EnemyCollisionPart& part : parts) {
        const SweepHit hit = CollisionManager::SweepSphere(
            bullet, movement, Sphere { part.position, part.radius });
        if (hit.isHit && hit.time <= targetHitTime) {
            return true;
        }
    }
    return false;
}

void CheckPlayerBulletsAgainstEnemy(Player& player, BaseEnemy& enemy,
    const std::vector<std::unique_ptr<BaseEnemy>>& enemies, bool isBoss = false)
{
    if (enemy.IsDead()) { return; }
    for (const auto& bullet : player.GetBullets()) {
        if (!bullet->IsAlive()) {
            continue;
        }
        const Sphere sphere { bullet->GetPreviousPosition(), bullet->GetCollisionRadius() };
        const Vector3 movement = bullet->GetPosition() - bullet->GetPreviousPosition();
        int32_t partIndex = 0;
        const SweepHit hit = enemy.SweepBullet(sphere, movement, partIndex);
        if (!hit.isHit) {
            continue;
        }
        bool blockedByEnemy = false;
        if (enemy.RequiresBulletOcclusionCheck()) {
            for (const auto& blocker : enemies) {
                if (blocker.get() != &enemy &&
                    EnemyBlocksBulletHit(blocker.get(), sphere, movement, hit.time)) {
                    blockedByEnemy = true;
                    break;
                }
            }
        }
        if (blockedByEnemy) {
            continue;
        }
        if (enemy.IsCollisionPartDamageable(partIndex)) {
            bullet->OnHitEnemy(hit.position);
            SoundManager::GetInstance()->Play("EnemyHit");
        } else {
            SoundManager::GetInstance()->Play("ArmorDeflect");
        }
        const bool wasDead = enemy.IsDead();
        enemy.OnBulletHit(partIndex, static_cast<float>(bullet->GetDamage()), hit.position);
        if (!wasDead && enemy.IsDead()) {
            SoundManager::GetInstance()->Play(isBoss ? "BossDestroyed" : "EnemyDestroyed");
        }
        bullet->SetDead();
    }
}

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
        if (player.ApplyDamage(bullet->GetDamage())) {
            EffectManager::GetInstance()->PlayEffect(
                "DamageHit",
                player.GetTranslate());
        }
        bullet->SetDead();
    }
}
}

void GameplayCollisionSystem::UpdateStageCollisions(
    Player& player,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects,
    std::vector<DestructibleLevelObject>& destructibles,
    Object3d* floorObject)
{
    const Vector3 playerPosition = player.GetTranslate();
    const Sphere playerSphere { playerPosition, kPlayerObstacleRadius };

    for (const std::unique_ptr<Object3d>& levelObject : levelObjects) {
        BoxCollider* collider = levelObject->GetCollider();
        if (collider == nullptr) {
            continue;
        }

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
        }
    }

    for (const std::unique_ptr<PlayerBullet>& bullet : player.GetBullets()) {
        if (!bullet->IsAlive()) {
            continue;
        }

        for (DestructibleLevelObject& destructible : destructibles) {
            if (destructible.destroyed || destructible.object == nullptr) {
                continue;
            }

            BoxCollider* collider = destructible.object->GetCollider();
            if (collider == nullptr) {
                continue;
            }

            const Sphere bulletSphere {
                bullet->GetPreviousPosition(),
                bullet->GetCollisionRadius()
            };
            const OBB destructibleBox = CollisionManager::MakeOBB(
                collider->GetCenter(),
                collider->GetSize(),
                collider->GetRotation());
            const Vector3 movement =
                bullet->GetPosition() - bullet->GetPreviousPosition();
            const SweepHit hit = CollisionManager::SweepSphere(
                bulletSphere,
                movement,
                destructibleBox);
            if (!hit.isHit) {
                continue;
            }

            destructible.hp -= static_cast<float>(bullet->GetDamage());
            bullet->SetDead();
            EffectManager::GetInstance()->PlayEffect("HitEffect", hit.position);

            if (destructible.hp <= 0.0f) {
                destructible.destroyed = true;
                const Vector3 center = collider->GetCenter();
                EffectManager::GetInstance()->PlayEffect("Explosion", center);
                CollisionManager::GetInstance()->UnregisterCollider(collider);
                destructible.object->SetCollider(nullptr);
                destructible.object->SetScale({ 0.0f, 0.0f, 0.0f });
                destructible.object->Update();
            }
            break;
        }

        if (!bullet->IsAlive()) {
            continue;
        }

        float floorY = -30.0f;
        if (floorObject != nullptr) {
            floorY = floorObject->GetTranslate().y;
        }
        if (bullet->GetPosition().y <= floorY) {
            Vector3 hitPosition = bullet->GetPosition();
            hitPosition.y = floorY;
            EffectManager::GetInstance()->PlayEffect("HitEffect", hitPosition);
            bullet->SetDead();
        }
    }
}

GameplayCollisionEvents GameplayCollisionSystem::UpdateCombatCollisions(
    Player& player,
    std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    BaseEnemy* boss,
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

            CheckPlayerBulletsAgainstEnemy(player, *enemy, enemies);
        }

    }

    if (boss != nullptr) {
        if (!boss->IsDead()) {
            CheckPlayerBulletsAgainstEnemy(player, *boss, enemies, true);
        }
    }
    CheckEnemyBulletsAgainstPlayer(player, independentBullets, events);
    return events;
}

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
