#include "Engine/CollisionManager/CollisionManager.h"

#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/Math/Collision.h"
#include "Engine/Math/Sphere.h"

std::unique_ptr<CollisionManager> CollisionManager::instance_ = nullptr;

namespace {
constexpr float kEnemyRaycastRadius = 2.5f;
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

    for (const std::unique_ptr<BaseEnemy>& enemy : *enemies_) {
        if (enemy == nullptr) {
            continue;
        }

        if (enemy->IsDead()) {
            continue;
        }

        Sphere enemySphere {};
        enemySphere.center = enemy->GetPosition();
        enemySphere.radius = kEnemyRaycastRadius;

        float distance = 0.0f;
        if (!RaySphereIntersect(normalizedRay, enemySphere, distance)) {
            continue;
        }

        if (!isHit || distance < nearestDistance) {
            isHit = true;
            nearestDistance = distance;
            nearestEnemy = enemy.get();
        }
    }

    if (!isHit) {
        return false;
    }

    hit.distance = nearestDistance;
    hit.enemy = nearestEnemy;
    hit.position = normalizedRay.origin + normalizedRay.direction * nearestDistance;

    return true;
}
