// ステージ・敵・弾・トリガーの衝突を調べ、得点や被弾演出に必要な結果を呼び出し元へ返す。
#include "App/Game/Collision/GameplayCollisionSystem.h"

#include "App/Game/Player/Player.h"
#include "App/Game/Player/Combat/Bullet/Common/PlayerBullet.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "App/Game/Enemy/DashEnemy/DashEnemy.h"
#include "App/Game/Enemy/Bullet/Common/EnemyBullet.h"
#include "Engine/audio/SoundManager.h"
#include "Engine/3D/Object3d.h"
#include "Engine/CollisionManager/BoxCollider.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Debug/Profiler/FpsMonitor.h"

#include <cmath>
#include <algorithm>
#include <limits>

namespace {
constexpr float kPlayerObstacleRadius = 1.0f;
constexpr float kPlayerEnemyCollisionRadius = 2.0f;

// 対象の型に依存せず、有効な部位のうち最も近い部位へ爆風を適用する。
template<class Target>
bool ApplyExplosionToTarget(Target& target, const Sphere& explosion, float damage, bool isDirectHit)
{
    if (isDirectHit || target.IsDead()) { return false; }
    std::vector<EnemyCollisionPart> parts;
    target.GetCollisionParts(parts);
    bool hasHitPart = false;
    int32_t nearestPartIndex = 0;
    float nearestDistance = (std::numeric_limits<float>::max)();
    for (const auto& part : parts) {
        if (!target.IsCollisionPartDamageable(part.partIndex)) { continue; }
        if (!part.IntersectSphere(explosion).isHit) { continue; }
        const float surfaceDistance = part.GetSurfaceDistance(explosion.center);
        if (!hasHitPart || surfaceDistance < nearestDistance) {
            hasHitPart = true;
            nearestPartIndex = part.partIndex;
            nearestDistance = surfaceDistance;
        }
    }
    if (!hasHitPart) { return false; }
    target.OnBulletHit(nearestPartIndex, damage, explosion.center);
    return target.IsDead();
}

// 弾の移動区間で最初に接触する障害物を探す。
SweepHit FindFirstObstacleHit(const Sphere& bullet, const Vector3& movement,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects)
{
    SweepHit nearest {};
    for (const auto& object : levelObjects) {
        for (size_t index = 0; index < object->GetColliderCount(); ++index) {
            const BoxCollider* collider = object->GetCollider(index);
            const OBB& box = collider->GetOBB();
            const SweepHit hit = CollisionManager::SweepSphere(bullet, movement, box);
            if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
                nearest = hit;
            }
        }
    }
    return nearest;
}

// 敵弾の移動区間とプレイヤーの当たり判定を取り、ダメージと命中演出を処理する。
void CheckEnemyBulletsAgainstPlayer(
    Player& player,
    const std::vector<std::unique_ptr<EnemyBullet>>& bullets,
    GameplayCollisionEvents& events,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects,
    bool collideWithObstacles)
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
        const SweepHit playerHit = CollisionManager::SweepSphere(
            bulletSphere,
            movement,
            playerSphere);
        SweepHit obstacleHit {};
        if (collideWithObstacles) {
            FpsScope obstacleScope("Collision.EnemyBulletObstacle");
            obstacleHit = FindFirstObstacleHit(bulletSphere, movement, levelObjects);
        }
        // 同時に接触した場合も障害物で遮る。
        if (obstacleHit.isHit && (!playerHit.isHit || obstacleHit.time <= playerHit.time)) {
            EffectManager::GetInstance()->PlayEffect("HitEffect", obstacleHit.position);
            bullet->SetDead();
            continue;
        }
        if (!playerHit.isHit) {
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

void GameplayCollisionSystem::UpdateEnemyMovementCollisions(
    std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects)
{
    FpsScope fpsScope("Collision.EnemyMovement");
    for (const auto& enemy : enemies) {
        auto* dash = dynamic_cast<DashEnemy*>(enemy.get());
        if (!dash || dash->IsDead()) { continue; }
        const auto& motion = dash->GetMotion();
        for (size_t index = 0; index < motion.segmentCount; ++index) {
            const auto& segment = motion.segments[index];
            if (!segment.attacking) { continue; }
            const SweepHit hit = FindFirstObstacleHit(Sphere { segment.start, DashEnemy::kAttackRadius },
                segment.end - segment.start, levelObjects);
            if (hit.isHit) {
                dash->StopAtObstacle(index, hit.time);
                EffectManager::GetInstance()->PlayEffect("HitEffect", dash->GetPosition());
                break;
            }
        }
        if (motion.IsWarning()) {
            const SweepHit wall = FindFirstObstacleHit(Sphere { motion.position, DashEnemy::kAttackRadius },
                motion.direction * motion.settings.dashDistance, levelObjects);
            dash->SetWarningLength(motion.settings.dashDistance * (wall.isHit ? wall.time : 1.0f));
        }
    }
}

// プレイヤーとステージ障害物の当たり判定を取り、接触への対応を行う。
void GameplayCollisionSystem::UpdateStageCollisions(
    Player& player,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects)
{
    FpsScope fpsScope("Collision.PlayerStage");
    const Vector3 playerPosition = player.GetTranslate();
    const Sphere playerSphere { playerPosition, kPlayerObstacleRadius };

    for (const std::unique_ptr<Object3d>& levelObject : levelObjects) {
      for (size_t colliderIndex = 0; colliderIndex < levelObject->GetColliderCount(); ++colliderIndex) {
        BoxCollider* collider = levelObject->GetCollider(colliderIndex);
        const OBB& obstacleBox = collider->GetOBB();
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
    CombatTarget* boss, std::vector<DestructibleLevelObject>& destructibles,
    float floorHeight, bool shouldCollideWithFloor,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects)
{
    PlayerBulletHit nearest {};
    for (DestructibleLevelObject& destructible : destructibles) {
        if (destructible.destroyed || destructible.object == nullptr) {
            continue;
        }
        for (size_t colliderIndex = 0; colliderIndex < destructible.object->GetColliderCount(); ++colliderIndex) {
            const BoxCollider* collider = destructible.object->GetCollider(colliderIndex);
            const OBB& box = collider->GetOBB();
            const SweepHit hit = CollisionManager::SweepSphere(bullet, movement, box);
            if (hit.isHit && (!nearest.hit.isHit || hit.time < nearest.hit.time)) {
                nearest.hit = hit;
                nearest.destructible = &destructible;
            }
        }
    }

    const SweepHit obstacleHit = FindFirstObstacleHit(bullet, movement, levelObjects);
    // 同じコライダーなら、先に調べた破壊可能物の処理を維持する。
    if (obstacleHit.isHit && (!nearest.hit.isHit || obstacleHit.time < nearest.hit.time)) {
        nearest.hit = obstacleHit;
        nearest.destructible = nullptr;
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

bool GameplayCollisionSystem::ApplyExplosionToEnemy(CombatTarget& enemy, const Sphere& explosion,
    float damage, const CombatTarget* directEnemy)
{
    return ApplyExplosionToTarget(enemy, explosion, damage, &enemy == directEnemy);
}

uint32_t GameplayCollisionSystem::ApplyExplosionDamage(const Vector3& position,
    const PlayerBulletExplosionSettings& settings, const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    CombatTarget* boss, const CombatTarget* directEnemy)
{
    if (!std::isfinite(settings.radius) || !std::isfinite(settings.damage) ||
        settings.radius <= 0.0f || settings.damage <= 0.0f) { return 0; }
    const Sphere explosion { position, settings.radius };
    uint32_t scoreAwarded = 0;
    for (const auto& enemy : enemies) {
        if (enemy.get() == boss) { continue; }
        if (ApplyExplosionToEnemy(*enemy, explosion, settings.damage, directEnemy)) {
            scoreAwarded += 100u;
            SoundManager::GetInstance()->Play("EnemyDestroyed");
        }
    }
    if (boss != nullptr && ApplyExplosionToEnemy(*boss, explosion, settings.damage, directEnemy)) {
        scoreAwarded += 1000u;
        SoundManager::GetInstance()->Play("BossDestroyed");
    }
    return scoreAwarded;
}

GameplayCollisionEvents GameplayCollisionSystem::UpdatePlayerBulletCollisions(
    Player& player, const std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    CombatTarget* boss, std::vector<DestructibleLevelObject>& destructibles,
    Object3d* floorObject, bool shouldCollideWithFloor,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects)
{
    FpsScope fpsScope("Collision.PlayerBullets");
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
            sphere, movement, enemies, boss, destructibles, floorHeight, shouldCollideWithFloor, levelObjects);
        if (!nearest.hit.isHit) {
            continue;
        }
        const auto explosionSettings = bullet->GetExplosionSettings();
        const bool hasExplosion = explosionSettings.radius > 0.0f;
        if (hasExplosion) { bullet->OnHitEnemy(nearest.hit.position); }
        if (nearest.enemy != nullptr) {
            CombatTarget& enemy = *nearest.enemy;
            if (enemy.IsCollisionPartDamageable(nearest.partIndex)) {
                if (!hasExplosion) { bullet->OnHitEnemy(nearest.hit.position); }
                if (bullet->GetDamage() > 0) {
                    // 部位中心へ埋まらないよう、弾の接触時の位置へ小さな火花を出す。
                    const Vector3 sparkPosition = sphere.center + movement * nearest.hit.time;
                    EffectManager::GetInstance()->PlayEffect("EnemyHitSpark", sparkPosition);
                }
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
            if (!hasExplosion) { EffectManager::GetInstance()->PlayEffect("HitEffect", nearest.hit.position); }
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
        if (hasExplosion) {
            events.scoreAwarded += ApplyExplosionDamage(nearest.hit.position, explosionSettings, enemies, boss, nearest.enemy);
        }
        bullet->SetDead();
    }
    return events;
}

// 敵本体と敵弾のプレイヤーへの接触を調べ、被弾演出の通知を集める。
GameplayCollisionEvents GameplayCollisionSystem::UpdateCombatCollisions(
    Player& player,
    std::vector<std::unique_ptr<BaseEnemy>>& enemies,
    const std::vector<std::unique_ptr<EnemyBullet>>& independentBullets,
    const std::vector<std::unique_ptr<Object3d>>& levelObjects,
    const Vector3* previousPlayerPosition,
    bool enemyBulletsCollideWithObstacles)
{
    FpsScope fpsScope("Collision.Combat");
    GameplayCollisionEvents events {};
    const Sphere playerSphere {
        player.GetTranslate(),
        kPlayerEnemyCollisionRadius * 0.5f
    };

    for (const std::unique_ptr<BaseEnemy>& enemy : enemies) {
        if (auto* dash = dynamic_cast<DashEnemy*>(enemy.get())) {
            const Vector3 previous = previousPlayerPosition ? *previousPlayerPosition : player.GetTranslate();
            const SweepHit hit = dash->FindPlayerContact(previous, player.GetTranslate());
            if (hit.isHit) {
                const Vector3 target = previous + (player.GetTranslate() - previous) * hit.time;
                // 薄い壁越しに二つの球が重なる場合も、中心間に壁があれば遮る。
                const SweepHit wall = FindFirstObstacleHit(Sphere { hit.position, 0.0f }, target - hit.position, levelObjects);
                if (!wall.isHit) {
                    dash->ConsumePlayerContact();
                    if (player.ApplyDamage(dash->GetMotion().settings.damage)) {
                        EffectManager::GetInstance()->PlayEffect("DamageHit", player.GetTranslate());
                    }
                }
            }
            continue;
        }
        if (!enemy->IsDead()) {
            std::vector<EnemyCollisionPart> collisionParts;
            enemy->GetCollisionParts(collisionParts);
            for (const EnemyCollisionPart& part : collisionParts) {
                if (part.IntersectSphere(playerSphere).isHit) {
                    OutputDebugStringA("Player Hit Enemy\n");
                    break;
                }
            }
        }
    }

    CheckEnemyBulletsAgainstPlayer(player, independentBullets, events, levelObjects, enemyBulletsCollideWithObstacles);
    return events;
}

// ステージ内のトリガーとの接触を調べ、ゲーム進行に使うイベントを返す。
GameplayCollisionEvents GameplayCollisionSystem::UpdateTriggers(
    Player& player,
    std::vector<StageTrigger>& triggers)
{
    FpsScope fpsScope("Collision.Triggers");
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
    const CombatTarget* boss)
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
