#pragma once

#include "Ray.h"
#include "Sphere.h"

bool RaySphereIntersect(
    const Ray& ray,
    const Sphere& sphere);

bool RaySphereIntersect(
    const Ray& ray,
    const Sphere& sphere,
    float& distance);

bool RayAabbIntersect(
    const Ray& ray,
    const Vector3& center,
    const Vector3& size,
    float& distance);
