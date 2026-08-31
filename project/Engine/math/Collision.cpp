#include "Collision.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
bool UpdateRaySlab(
    float origin,
    float direction,
    float minimum,
    float maximum,
    float& nearDistance,
    float& farDistance)
{
    constexpr float kDirectionEpsilon = 0.000001f;
    if (std::abs(direction) <= kDirectionEpsilon) {
        return origin >= minimum && origin <= maximum;
    }

    float inverseDirection = 1.0f / direction;
    float firstDistance = (minimum - origin) * inverseDirection;
    float secondDistance = (maximum - origin) * inverseDirection;
    if (firstDistance > secondDistance) {
        std::swap(firstDistance, secondDistance);
    }

    if (firstDistance > nearDistance) {
        nearDistance = firstDistance;
    }
    if (secondDistance < farDistance) {
        farDistance = secondDistance;
    }

    return nearDistance <= farDistance;
}
}

bool RaySphereIntersect(const Ray& ray, const Sphere& sphere)
{
    float distance = 0.0f;
    return RaySphereIntersect(ray, sphere, distance);
}

bool RaySphereIntersect(const Ray& ray, const Sphere& sphere, float& distance)
{
    Vector3 sphereToRay = ray.origin - sphere.center;
    Vector3 direction = Normalize(ray.direction);

    float a = Dot(direction, direction);

    if (a <= 0.0f) {
        return false;
    }

    float b = 2.0f * Dot(sphereToRay, direction);

    float c = Dot(sphereToRay, sphereToRay) - sphere.radius * sphere.radius;

    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return false;
    }

    float sqrtDiscriminant = std::sqrt(discriminant);
    float denominator = 2.0f * a;
    float nearDistance = (-b - sqrtDiscriminant) / denominator;
    float farDistance = (-b + sqrtDiscriminant) / denominator;

    if (nearDistance >= 0.0f) {
        distance = nearDistance;
        return true;
    }

    if (farDistance >= 0.0f) {
        distance = farDistance;
        return true;
    }

    return false;
}

bool RayAabbIntersect(
    const Ray& ray,
    const Vector3& center,
    const Vector3& size,
    float& distance)
{
    Vector3 direction = Normalize(ray.direction);
    if (Dot(direction, direction) <= 0.0f) {
        return false;
    }

    Vector3 halfSize = {
        std::abs(size.x) * 0.5f,
        std::abs(size.y) * 0.5f,
        std::abs(size.z) * 0.5f
    };
    Vector3 minimum = center - halfSize;
    Vector3 maximum = center + halfSize;

    float nearDistance = 0.0f;
    float farDistance = (std::numeric_limits<float>::max)();

    if (!UpdateRaySlab(ray.origin.x, direction.x, minimum.x, maximum.x, nearDistance, farDistance)) {
        return false;
    }
    if (!UpdateRaySlab(ray.origin.y, direction.y, minimum.y, maximum.y, nearDistance, farDistance)) {
        return false;
    }
    if (!UpdateRaySlab(ray.origin.z, direction.z, minimum.z, maximum.z, nearDistance, farDistance)) {
        return false;
    }

    if (farDistance < 0.0f) {
        return false;
    }

    distance = nearDistance;
    return true;
}
