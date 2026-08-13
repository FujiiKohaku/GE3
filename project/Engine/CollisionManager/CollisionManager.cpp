#include "Engine/CollisionManager/CollisionManager.h"

#include "Engine/CollisionManager/BoxCollider.h"
#include "Engine/Math/Collision.h"
#include "Engine/Math/MatrixMath.h"
#include "Engine/Math/Sphere.h"

#include <algorithm>
#include <cmath>

std::unique_ptr<CollisionManager> CollisionManager::instance_ = nullptr;

namespace {
float ClampValue(float value, float minimum, float maximum)
{
    return (std::max)(minimum, (std::min)(value, maximum));
}

Vector3 AbsoluteHalfSize(const Vector3& size)
{
    return {
        std::abs(size.x) * 0.5f,
        std::abs(size.y) * 0.5f,
        std::abs(size.z) * 0.5f
    };
}

bool RayObbIntersect(const Ray& ray, const OBB& box, float& distance)
{
    const Vector3 offset = ray.origin - box.center;
    Ray localRay {};
    localRay.origin = {
        Dot(offset, box.orientation[0]),
        Dot(offset, box.orientation[1]),
        Dot(offset, box.orientation[2])
    };
    localRay.direction = {
        Dot(ray.direction, box.orientation[0]),
        Dot(ray.direction, box.orientation[1]),
        Dot(ray.direction, box.orientation[2])
    };
    return RayAabbIntersect(
        localRay,
        { 0.0f, 0.0f, 0.0f },
        box.size,
        distance);
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

void CollisionManager::ClearRaycastSphereTargets()
{
    raycastSphereTargets_.clear();
}

void CollisionManager::RegisterRaycastSphereTarget(
    CollisionObjectId objectId,
    const Sphere& sphere,
    CollisionLayer category)
{
    raycastSphereTargets_.push_back({ objectId, sphere, category });
}

bool CollisionManager::Raycast(
    const Ray& ray,
    RaycastHit& hit,
    CollisionLayer queryMask) const
{
    Ray normalizedRay = ray;
    normalizedRay.direction = Normalize(ray.direction);

    if (Dot(normalizedRay.direction, normalizedRay.direction) <= 0.0f) {
        return false;
    }

    bool isHit = false;
    float nearestDistance = 0.0f;
    CollisionObjectId nearestObjectId = kInvalidCollisionObjectId;
    BoxCollider* nearestCollider = nullptr;
    Vector3 nearestPosition = { 0.0f, 0.0f, 0.0f };

    for (const RaycastSphereTarget& target : raycastSphereTargets_) {
        if ((target.category & queryMask) == 0) {
            continue;
        }
        float distance = 0.0f;
        if (!RaySphereIntersect(normalizedRay, target.sphere, distance)) {
            continue;
        }
        if (!isHit || distance < nearestDistance) {
            isHit = true;
            nearestDistance = distance;
            nearestObjectId = target.objectId;
            nearestCollider = nullptr;
            nearestPosition =
                normalizedRay.origin + normalizedRay.direction * distance;
        }
    }

    for (const std::unique_ptr<BoxCollider>& collider : colliders_) {
        if ((collider->GetCategory() & queryMask) == 0) {
            continue;
        }
        float distance = 0.0f;
        const OBB box = MakeOBB(
            collider->GetCenter(),
            collider->GetSize(),
            collider->GetRotation());
        if (!RayObbIntersect(normalizedRay, box, distance)) {
            continue;
        }

        if (!isHit || distance < nearestDistance) {
            isHit = true;
            nearestDistance = distance;
            nearestObjectId = kInvalidCollisionObjectId;
            nearestCollider = collider.get();
            nearestPosition = normalizedRay.origin + normalizedRay.direction * distance;
        }
    }

    if (!isHit) {
        return false;
    }

    hit.distance = nearestDistance;
    hit.objectId = nearestObjectId;
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

CollisionHit CollisionManager::Intersect(
    const Sphere& first,
    const Sphere& second)
{
    CollisionHit hit {};
    const float firstRadius = std::abs(first.radius);
    const float secondRadius = std::abs(second.radius);
    const float combinedRadius = firstRadius + secondRadius;
    const Vector3 difference = second.center - first.center;
    const float distanceSquared = Dot(difference, difference);
    if (distanceSquared > combinedRadius * combinedRadius) {
        return hit;
    }

    const float distance = std::sqrt((std::max)(distanceSquared, 0.0f));
    hit.isHit = true;
    hit.normal = distance > 0.000001f
        ? difference * (1.0f / distance)
        : Vector3 { 0.0f, 1.0f, 0.0f };
    hit.penetration = combinedRadius - distance;
    hit.position = first.center + hit.normal *
        (firstRadius - hit.penetration * 0.5f);
    return hit;
}

CollisionHit CollisionManager::Intersect(
    const Sphere& sphere,
    const AABB& box)
{
    CollisionHit hit {};
    const float radius = std::abs(sphere.radius);
    const Vector3 halfSize = AbsoluteHalfSize(box.size);
    const Vector3 minimum = box.center - halfSize;
    const Vector3 maximum = box.center + halfSize;
    const Vector3 closest = {
        ClampValue(sphere.center.x, minimum.x, maximum.x),
        ClampValue(sphere.center.y, minimum.y, maximum.y),
        ClampValue(sphere.center.z, minimum.z, maximum.z)
    };
    const Vector3 difference = sphere.center - closest;
    const float distanceSquared = Dot(difference, difference);
    if (distanceSquared > radius * radius) {
        return hit;
    }

    hit.isHit = true;
    hit.position = closest;
    if (distanceSquared > 0.000001f) {
        const float distance = std::sqrt(distanceSquared);
        hit.normal = difference * (1.0f / distance);
        hit.penetration = radius - distance;
        return hit;
    }

    const float faceDistances[6] = {
        sphere.center.x - minimum.x,
        maximum.x - sphere.center.x,
        sphere.center.y - minimum.y,
        maximum.y - sphere.center.y,
        sphere.center.z - minimum.z,
        maximum.z - sphere.center.z
    };
    int nearestFace = 0;
    for (int index = 1; index < 6; ++index) {
        if (faceDistances[index] < faceDistances[nearestFace]) {
            nearestFace = index;
        }
    }
    static constexpr Vector3 kFaceNormals[6] = {
        { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }
    };
    hit.normal = kFaceNormals[nearestFace];
    hit.penetration = radius + faceDistances[nearestFace];
    hit.position = sphere.center + hit.normal * faceDistances[nearestFace];
    return hit;
}

CollisionHit CollisionManager::Intersect(
    const Sphere& sphere,
    const OBB& box)
{
    CollisionHit hit {};
    const Vector3 halfSize = AbsoluteHalfSize(box.size);
    const Vector3 offset = sphere.center - box.center;
    const Vector3 localCenter = {
        Dot(offset, box.orientation[0]),
        Dot(offset, box.orientation[1]),
        Dot(offset, box.orientation[2])
    };
    const Vector3 closestLocal = {
        ClampValue(localCenter.x, -halfSize.x, halfSize.x),
        ClampValue(localCenter.y, -halfSize.y, halfSize.y),
        ClampValue(localCenter.z, -halfSize.z, halfSize.z)
    };
    const Vector3 localDifference = localCenter - closestLocal;
    const float radius = std::abs(sphere.radius);
    const float distanceSquared = Dot(localDifference, localDifference);
    if (distanceSquared > radius * radius) {
        return hit;
    }

    const auto toWorldDirection = [&box](const Vector3& local) {
        return box.orientation[0] * local.x +
            box.orientation[1] * local.y +
            box.orientation[2] * local.z;
    };
    hit.isHit = true;
    hit.position = box.center + toWorldDirection(closestLocal);
    if (distanceSquared > 0.000001f) {
        const float distance = std::sqrt(distanceSquared);
        hit.normal = toWorldDirection(localDifference * (1.0f / distance));
        hit.penetration = radius - distance;
        return hit;
    }

    const float faceDistances[6] = {
        localCenter.x + halfSize.x,
        halfSize.x - localCenter.x,
        localCenter.y + halfSize.y,
        halfSize.y - localCenter.y,
        localCenter.z + halfSize.z,
        halfSize.z - localCenter.z
    };
    int nearestFace = 0;
    for (int index = 1; index < 6; ++index) {
        if (faceDistances[index] < faceDistances[nearestFace]) {
            nearestFace = index;
        }
    }
    static constexpr Vector3 kLocalFaceNormals[6] = {
        { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }
    };
    hit.normal = toWorldDirection(kLocalFaceNormals[nearestFace]);
    hit.penetration = radius + faceDistances[nearestFace];
    hit.position = sphere.center + hit.normal * faceDistances[nearestFace];
    return hit;
}

CollisionHit CollisionManager::Intersect(
    const AABB& first,
    const AABB& second)
{
    CollisionHit hit {};
    const Vector3 firstHalfSize = AbsoluteHalfSize(first.size);
    const Vector3 secondHalfSize = AbsoluteHalfSize(second.size);
    const Vector3 difference = second.center - first.center;
    const Vector3 overlap = {
        firstHalfSize.x + secondHalfSize.x - std::abs(difference.x),
        firstHalfSize.y + secondHalfSize.y - std::abs(difference.y),
        firstHalfSize.z + secondHalfSize.z - std::abs(difference.z)
    };
    if (overlap.x < 0.0f || overlap.y < 0.0f || overlap.z < 0.0f) {
        return hit;
    }

    hit.isHit = true;
    hit.penetration = overlap.x;
    hit.normal = { difference.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f };
    if (overlap.y < hit.penetration) {
        hit.penetration = overlap.y;
        hit.normal = { 0.0f, difference.y < 0.0f ? -1.0f : 1.0f, 0.0f };
    }
    if (overlap.z < hit.penetration) {
        hit.penetration = overlap.z;
        hit.normal = { 0.0f, 0.0f, difference.z < 0.0f ? -1.0f : 1.0f };
    }
    hit.position = (first.center + second.center) * 0.5f;
    return hit;
}

OBB CollisionManager::MakeOBB(
    const Vector3& center,
    const Vector3& size,
    const Vector3& rotation)
{
    const Matrix4x4 rotationMatrix = MatrixMath::Multiply(
        MatrixMath::Multiply(
            MatrixMath::MakeRotateXMatrix(rotation.x),
            MatrixMath::MakeRotateYMatrix(rotation.y)),
        MatrixMath::MakeRotateZMatrix(rotation.z));
    OBB box {};
    box.center = center;
    box.size = size;
    box.orientation[0] = Normalize(Vector3 {
        rotationMatrix.m[0][0],
        rotationMatrix.m[0][1],
        rotationMatrix.m[0][2]
    });
    box.orientation[1] = Normalize(Vector3 {
        rotationMatrix.m[1][0],
        rotationMatrix.m[1][1],
        rotationMatrix.m[1][2]
    });
    box.orientation[2] = Normalize(Vector3 {
        rotationMatrix.m[2][0],
        rotationMatrix.m[2][1],
        rotationMatrix.m[2][2]
    });
    return box;
}

SweepHit CollisionManager::SweepSphere(
    const Sphere& sphere,
    const Vector3& movement,
    const Sphere& target)
{
    SweepHit sweep {};
    const CollisionHit initialHit = Intersect(sphere, target);
    if (initialHit.isHit) {
        static_cast<CollisionHit&>(sweep) = initialHit;
        sweep.time = 0.0f;
        return sweep;
    }

    const float movementLengthSquared = Dot(movement, movement);
    if (movementLengthSquared <= 0.000001f) {
        return sweep;
    }
    const float movementLength = std::sqrt(movementLengthSquared);
    const Ray ray { sphere.center, movement * (1.0f / movementLength) };
    const Sphere expandedTarget {
        target.center,
        std::abs(target.radius) + std::abs(sphere.radius)
    };
    float distance = 0.0f;
    if (!RaySphereIntersect(ray, expandedTarget, distance) ||
        distance > movementLength) {
        return sweep;
    }

    const Vector3 impactCenter = ray.origin + ray.direction * distance;
    const Vector3 outward = NormalizeSafe(impactCenter - target.center);
    sweep.isHit = true;
    sweep.time = distance / movementLength;
    sweep.normal = outward;
    sweep.position = impactCenter - outward * std::abs(sphere.radius);
    sweep.penetration = 0.0f;
    return sweep;
}

SweepHit CollisionManager::SweepSphere(
    const Sphere& sphere,
    const Vector3& movement,
    const OBB& target)
{
    SweepHit sweep {};
    const CollisionHit initialHit = Intersect(sphere, target);
    if (initialHit.isHit) {
        static_cast<CollisionHit&>(sweep) = initialHit;
        sweep.time = 0.0f;
        return sweep;
    }

    const float movementLengthSquared = Dot(movement, movement);
    if (movementLengthSquared <= 0.000001f) {
        return sweep;
    }
    const float movementLength = std::sqrt(movementLengthSquared);
    const Ray ray { sphere.center, movement * (1.0f / movementLength) };
    OBB expandedTarget = target;
    const float radius = std::abs(sphere.radius);
    expandedTarget.size.x += radius * 2.0f;
    expandedTarget.size.y += radius * 2.0f;
    expandedTarget.size.z += radius * 2.0f;
    float distance = 0.0f;
    if (!RayObbIntersect(ray, expandedTarget, distance) ||
        distance > movementLength) {
        return sweep;
    }

    const Vector3 impactCenter = ray.origin + ray.direction * distance;
    const Sphere impactSphere { impactCenter, sphere.radius };
    const CollisionHit impact = Intersect(impactSphere, target);
    sweep.isHit = true;
    sweep.time = distance / movementLength;
    sweep.position = impact.isHit ? impact.position : impactCenter;
    sweep.normal = impact.isHit ? impact.normal : ray.direction * -1.0f;
    sweep.penetration = impact.isHit ? impact.penetration : 0.0f;
    return sweep;
}
