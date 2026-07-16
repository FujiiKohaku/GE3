#include "Engine/CollisionManager/CollisionManager.h"

#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/Math/Collision.h"
#include "Engine/Math/Sphere.h"

std::unique_ptr<CollisionManager> CollisionManager::instance_ = nullptr;

namespace {
void RaycastEnemyParts(
    BaseEnemy* enemy,
    const Ray& normalizedRay,
    bool& isHit,
    float& nearestDistance,
    BaseEnemy*& nearestEnemy,
    Vector3& nearestPosition)
{
    if (enemy == nullptr) {
        return;
    }

    if (enemy->IsDead()) {
        return;
    }

    std::vector<EnemyCollisionPart> enemyCollisionParts;
    enemy->GetCollisionParts(enemyCollisionParts);

    for (const EnemyCollisionPart& part : enemyCollisionParts) {
        Sphere enemySphere {};
        enemySphere.center = part.position;
        enemySphere.radius = part.radius;

        float distance = 0.0f;
        if (!RaySphereIntersect(normalizedRay, enemySphere, distance)) {
            continue;
        }

        if (!isHit || distance < nearestDistance) {
            isHit = true;
            nearestDistance = distance;
            nearestEnemy = enemy;
            nearestPosition = part.position;
        }
    }
}
}

CollisionManager::CollisionManager(ConstructorKey)
{
}

CollisionManager* CollisionManager::GetInstance()
{
    if (!instance_) {
        instance_ = std::make_unique<CollisionManager>(ConstructorKey());
    }

    return instance_.get();
}

void CollisionManager::Finalize()
{
    instance_.reset();
}

void CollisionManager::SetEnemies(const EnemyList* enemies)
{
    enemies_ = enemies;
}

void CollisionManager::SetBoss(BaseEnemy* boss)
{
    boss_ = boss;
}

bool CollisionManager::Raycast(const Ray& ray, RaycastHit& hit) const
{
    if (enemies_ == nullptr) {
        return false;
    }

    Ray normalizedRay = ray;
    normalizedRay.direction = Normalize(ray.direction);

    if (Dot(normalizedRay.direction, normalizedRay.direction) <= 0.0f) {
        return false;
    }

    bool isHit = false;
    float nearestDistance = 0.0f;
    BaseEnemy* nearestEnemy = nullptr;
    Vector3 nearestPosition = { 0.0f, 0.0f, 0.0f };

    for (const std::unique_ptr<BaseEnemy>& enemy : *enemies_) {
        RaycastEnemyParts(
            enemy.get(),
            normalizedRay,
            isHit,
            nearestDistance,
            nearestEnemy,
            nearestPosition);
    }

    RaycastEnemyParts(
        boss_,
        normalizedRay,
        isHit,
        nearestDistance,
        nearestEnemy,
        nearestPosition);

    if (!isHit) {
        return false;
    }

    hit.distance = nearestDistance;
    hit.enemy = nearestEnemy;
    hit.position = nearestPosition;

    return true;
}

void CollisionManager::RegisterCollider(BoxCollider*)
{
}

void CollisionManager::UnregisterCollider(BoxCollider*)
{
}
