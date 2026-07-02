#include "Collision.h"
#include <cmath>

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
