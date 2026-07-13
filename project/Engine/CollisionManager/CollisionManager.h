#pragma once

#include "Engine/Math/Ray.h"
#include <memory>
#include <vector>

class BaseEnemy;

struct RaycastHit {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    float distance = 0.0f;
    BaseEnemy* enemy = nullptr;
};

class CollisionManager {
public:
    using EnemyList = std::vector<std::unique_ptr<BaseEnemy>>;

    static CollisionManager* GetInstance();
    static void Finalize();

    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    void SetEnemies(const EnemyList* enemies);
    void SetBoss(BaseEnemy* boss);
    bool Raycast(const Ray& ray, RaycastHit& hit) const;

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class CollisionManager;
    };

    explicit CollisionManager(ConstructorKey);
    ~CollisionManager() = default;

private:
    static std::unique_ptr<CollisionManager> instance_;

    const EnemyList* enemies_ = nullptr;
    BaseEnemy* boss_ = nullptr;
};
