#include "Engine/CollisionManager/CollisionManager.h"

#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/Math/Collision.h"
#include "Engine/Math/Sphere.h"

std::unique_ptr<CollisionManager> CollisionManager::instance_ = nullptr;

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
    std::vector<EnemyCollisionPart> enemyCollisionParts;

    for (const std::unique_ptr<BaseEnemy>& enemy : *enemies_) {
        if (enemy == nullptr) {
            continue;
        }

        if (enemy->IsDead()) {
            continue;
        }

        enemyCollisionParts.clear();
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
                nearestEnemy = enemy.get();
                nearestPosition = part.position;
            }
        }
    }

    if (!isHit) {
        return false;
    }

    hit.distance = nearestDistance;
    hit.enemy = nearestEnemy;
    hit.position = nearestPosition;

    return true;
}
