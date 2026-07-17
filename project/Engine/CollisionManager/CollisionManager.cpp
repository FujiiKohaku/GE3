#include "Engine/CollisionManager/CollisionManager.h"

#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/CollisionManager/BoxCollider.h"
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

CollisionManager::~CollisionManager() = default;

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
    BoxCollider* nearestCollider = nullptr;
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

    for (const std::unique_ptr<BoxCollider>& collider : colliders_) {
        float distance = 0.0f;
        if (!RayAabbIntersect(
                normalizedRay,
                collider->GetCenter(),
                collider->GetSize(),
                distance)) {
            continue;
        }

        if (!isHit || distance < nearestDistance) {
            isHit = true;
            nearestDistance = distance;
            nearestEnemy = nullptr;
            nearestCollider = collider.get();
            nearestPosition = normalizedRay.origin + normalizedRay.direction * distance;
        }
    }

    if (!isHit) {
        return false;
    }

    hit.distance = nearestDistance;
    hit.enemy = nearestEnemy;
    hit.collider = nearestCollider;
    hit.position = nearestPosition;

    return true;
}

BoxCollider* CollisionManager::RegisterCollider(std::unique_ptr<BoxCollider> collider)
{
    if (!collider) {
        return nullptr;
    }

    BoxCollider* registeredCollider = collider.get();
    colliders_.push_back(std::move(collider));
    return registeredCollider;
}

void CollisionManager::UnregisterCollider(BoxCollider* collider)
{
    for (std::vector<std::unique_ptr<BoxCollider>>::iterator iterator = colliders_.begin();
        iterator != colliders_.end();
        ++iterator) {
        if (iterator->get() == collider) {
            colliders_.erase(iterator);
            return;
        }
    }
}
