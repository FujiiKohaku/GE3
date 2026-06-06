#include "Collision.h"
#include <cmath>

bool RaySphereIntersect(const Ray& ray,const Sphere& sphere)
{
    Vector3 sphereToRay = ray.origin - sphere.center;

    float a = Dot(ray.direction, ray.direction);

    float b = 2.0f * Dot(sphereToRay, ray.direction);

    float c = Dot(sphereToRay, sphereToRay) - sphere.radius * sphere.radius;

    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return false;
    }

    return true;
}