#pragma once

#include "Engine/Math/Ray.h"
#include "Engine/Math/Sphere.h"
#include "Engine/CollisionManager/CollisionLayer.h"
#include <cstdint>
#include <memory>
#include <vector>

class BoxCollider;

using CollisionObjectId = uint64_t;
constexpr CollisionObjectId kInvalidCollisionObjectId = 0;

struct AABB {
    Vector3 center = { 0.0f, 0.0f, 0.0f };
    Vector3 size = { 1.0f, 1.0f, 1.0f };
};

struct OBB {
    Vector3 center = { 0.0f, 0.0f, 0.0f };
    Vector3 size = { 1.0f, 1.0f, 1.0f };
    Vector3 orientation[3] = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    };
};

struct CollisionHit {
    bool isHit = false;
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 normal = { 0.0f, 0.0f, 0.0f };
    float penetration = 0.0f;
};

struct SweepHit : CollisionHit {
    float time = 1.0f;
};

struct RaycastHit {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    float distance = 0.0f;
    CollisionObjectId objectId = kInvalidCollisionObjectId;
    BoxCollider* collider = nullptr;
};

struct RaycastSphereTarget {
    CollisionObjectId objectId = kInvalidCollisionObjectId;
    Sphere sphere {};
    CollisionLayer category = 1u;
};

class CollisionManager {
public:
    static CollisionManager* GetInstance();
    static void Finalize();

    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    void ClearRaycastSphereTargets();
    void RegisterRaycastSphereTarget(
        CollisionObjectId objectId,
        const Sphere& sphere,
        CollisionLayer category = 1u);
    BoxCollider* RegisterCollider(std::unique_ptr<BoxCollider> collider);
    void UnregisterCollider(BoxCollider* collider);
    bool Raycast(
        const Ray& ray,
        RaycastHit& hit,
        CollisionLayer queryMask = kCollisionLayerAll) const;

    static CollisionHit Intersect(const Sphere& first, const Sphere& second);
    static CollisionHit Intersect(const Sphere& sphere, const AABB& box);
    static CollisionHit Intersect(const Sphere& sphere, const OBB& box);
    static CollisionHit Intersect(const AABB& first, const AABB& second);
    static OBB MakeOBB(
        const Vector3& center,
        const Vector3& size,
        const Vector3& rotation);
    static SweepHit SweepSphere(
        const Sphere& sphere,
        const Vector3& movement,
        const Sphere& target);
    static SweepHit SweepSphere(
        const Sphere& sphere,
        const Vector3& movement,
        const OBB& target);

public:
    class ConstructorKey {
        ConstructorKey() = default;
        friend class CollisionManager;
    };

    explicit CollisionManager(ConstructorKey);
    ~CollisionManager();

private:
    static std::unique_ptr<CollisionManager> instance_;

    std::vector<RaycastSphereTarget> raycastSphereTargets_;
    std::vector<std::unique_ptr<BoxCollider>> colliders_;
};
