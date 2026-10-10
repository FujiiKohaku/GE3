#pragma once
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

struct DxrSceneBounds {
    Vector3 center = {};
    float radius = -1.0f;
};

inline bool IsValidDxrSceneBounds(const DxrSceneBounds& bounds) {
    return std::isfinite(bounds.center.x) && std::isfinite(bounds.center.y) && std::isfinite(bounds.center.z)
        && std::isfinite(bounds.radius) && bounds.radius >= 0;
}

inline DxrSceneBounds TransformDxrSceneBounds(const DxrSceneBounds& localBounds, const Matrix4x4& world) {
    DxrSceneBounds bounds;
    if (!IsValidDxrSceneBounds(localBounds)) { return bounds; }
    bounds.center = MatrixMath::Transform(localBounds.center, world);
    // The Frobenius norm bounds scale even with shear and nonuniform transforms.
    float scaleSquared = 0;
    for (uint32_t row = 0; row < 3; ++row) {
        for (uint32_t column = 0; column < 3; ++column) { scaleSquared += world.m[row][column] * world.m[row][column]; }
    }
    bounds.radius = localBounds.radius * std::sqrt(scaleSquared);
    return bounds;
}

inline bool IsDxrSceneBoundsVisible(const DxrSceneBounds& bounds, const Matrix4x4& viewProjection) {
    if (!IsValidDxrSceneBounds(bounds)) { return true; }
    for (uint32_t row = 0; row < 4; ++row) {
        for (uint32_t column = 0; column < 4; ++column) {
            if (!std::isfinite(viewProjection.m[row][column])) { return true; }
        }
    }
    // Row-vector convention, D3D clip depth 0..w. Invalid planes keep the object.
    for (uint32_t planeIndex = 0; planeIndex < 6; ++planeIndex) {
        Vector4 plane = {};
        for (uint32_t row = 0; row < 4; ++row) {
            float value = viewProjection.m[row][3];
            if (planeIndex == 0) { value += viewProjection.m[row][0]; }
            if (planeIndex == 1) { value -= viewProjection.m[row][0]; }
            if (planeIndex == 2) { value += viewProjection.m[row][1]; }
            if (planeIndex == 3) { value -= viewProjection.m[row][1]; }
            if (planeIndex == 4) { value = viewProjection.m[row][2]; }
            if (planeIndex == 5) { value -= viewProjection.m[row][2]; }
            if (row == 0) { plane.x = value; }
            if (row == 1) { plane.y = value; }
            if (row == 2) { plane.z = value; }
            if (row == 3) { plane.w = value; }
        }
        float normalLength = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        float signedDistance = plane.x * bounds.center.x + plane.y * bounds.center.y + plane.z * bounds.center.z + plane.w;
        if (std::isfinite(normalLength) && std::isfinite(signedDistance) && normalLength > 0.000001f
            && signedDistance < -bounds.radius * normalLength) { return false; }
    }
    return true;
}
