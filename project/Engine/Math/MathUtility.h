#pragma once
#include "MathStruct.h"
#include <algorithm>
#include <cmath>

namespace MathUtility {
inline Vector3 CrossVector(const Vector3& first, const Vector3& second)
{
    return {
        first.y * second.z - first.z * second.y,
        first.z * second.x - first.x * second.z,
        first.x * second.y - first.y * second.x
    };
}

inline Vector3 TransformDirection(
    const Vector3& direction,
    const Matrix4x4& matrix)
{
    return {
        direction.x * matrix.m[0][0] +
            direction.y * matrix.m[1][0] +
            direction.z * matrix.m[2][0],
        direction.x * matrix.m[0][1] +
            direction.y * matrix.m[1][1] +
            direction.z * matrix.m[2][1],
        direction.x * matrix.m[0][2] +
            direction.y * matrix.m[1][2] +
            direction.z * matrix.m[2][2]
    };
}

inline Vector3 GetMatrixTranslation(const Matrix4x4& matrix)
{
    return {
        matrix.m[3][0],
        matrix.m[3][1],
        matrix.m[3][2]
    };
}

inline Quaternion MultiplyQuaternion(
    const Quaternion& first,
    const Quaternion& second)
{
    return {
        first.w * second.x +
            first.x * second.w +
            first.y * second.z -
            first.z * second.y,
        first.w * second.y -
            first.x * second.z +
            first.y * second.w +
            first.z * second.x,
        first.w * second.z +
            first.x * second.y -
            first.y * second.x +
            first.z * second.w,
        first.w * second.w -
            first.x * second.x -
            first.y * second.y -
            first.z * second.z
    };
}

inline Quaternion QuaternionFromTo(
    const Vector3& fromDirection,
    const Vector3& toDirection)
{
    Vector3 from = NormalizeSafe(fromDirection);
    Vector3 to = NormalizeSafe(toDirection);
    float directionDot = std::clamp(Dot(from, to), -1.0f, 1.0f);

    if (directionDot >= 0.9999f) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    if (directionDot <= -0.9999f) {
        Vector3 rotationAxis =
            CrossVector(from, { 1.0f, 0.0f, 0.0f });
        if (Vector3LengthSquared(rotationAxis) <= 0.0001f) {
            rotationAxis =
                CrossVector(from, { 0.0f, 1.0f, 0.0f });
        }
        rotationAxis = NormalizeSafe(rotationAxis);
        return {
            rotationAxis.x,
            rotationAxis.y,
            rotationAxis.z,
            0.0f
        };
    }

    Vector3 rotationAxis = CrossVector(from, to);
    Quaternion rotation = {
        rotationAxis.x,
        rotationAxis.y,
        rotationAxis.z,
        1.0f + directionDot
    };
    return Normalize(rotation);
}

inline float MoveTowardFloat(
    float currentValue,
    float targetValue,
    float maximumDelta)
{
    if (currentValue < targetValue) {
        const float increasedValue =
            currentValue + maximumDelta;
        if (increasedValue > targetValue) {
            return targetValue;
        }
        return increasedValue;
    }
    if (currentValue > targetValue) {
        const float decreasedValue =
            currentValue - maximumDelta;
        if (decreasedValue < targetValue) {
            return targetValue;
        }
        return decreasedValue;
    }
    return targetValue;
}

inline bool TrySampleTriangleSurface(
    const Vector3& vertexA,
    const Vector3& vertexB,
    const Vector3& vertexC,
    float worldX,
    float worldZ,
    float& surfaceHeight,
    Vector3& surfaceNormal)
{
    const float denominator =
        (vertexB.z - vertexC.z) *
            (vertexA.x - vertexC.x) +
        (vertexC.x - vertexB.x) *
            (vertexA.z - vertexC.z);
    if (std::abs(denominator) <= 0.000001f) {
        return false;
    }

    const float weightA =
        ((vertexB.z - vertexC.z) *
             (worldX - vertexC.x) +
         (vertexC.x - vertexB.x) *
             (worldZ - vertexC.z)) /
        denominator;
    const float weightB =
        ((vertexC.z - vertexA.z) *
             (worldX - vertexC.x) +
         (vertexA.x - vertexC.x) *
             (worldZ - vertexC.z)) /
        denominator;
    const float weightC = 1.0f - weightA - weightB;
    const float tolerance = -0.0001f;
    if (weightA < tolerance ||
        weightB < tolerance ||
        weightC < tolerance) {
        return false;
    }

    surfaceHeight =
        vertexA.y * weightA +
        vertexB.y * weightB +
        vertexC.y * weightC;
    surfaceNormal =
        NormalizeSafe(
            CrossVector(
                vertexB - vertexA,
                vertexC - vertexA));
    if (surfaceNormal.y < 0.0f) {
        surfaceNormal = surfaceNormal * -1.0f;
    }
    return true;
}

inline Vector4 LerpColor(const Vector4& from, const Vector4& to, float weight)
{
    return {
        from.x + (to.x - from.x) * weight,
        from.y + (to.y - from.y) * weight,
        from.z + (to.z - from.z) * weight,
        from.w + (to.w - from.w) * weight
    };
}
}
