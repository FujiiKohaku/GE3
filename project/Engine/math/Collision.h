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
