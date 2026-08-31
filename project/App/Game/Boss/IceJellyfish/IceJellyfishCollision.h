#pragma once

#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Math/MatrixMath.h"
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

// CPU-only geometry shared by gameplay and collision verification.
namespace IceJellyfishCollision {
inline OBB TransformBox(const Matrix4x4& world, const Vector3& center, const Vector3& size)
{
    OBB box {};
    box.center = MatrixMath::Transform(center, world);
    const Vector3 x = { world.m[0][0], world.m[0][1], world.m[0][2] };
    const Vector3 y = { world.m[1][0], world.m[1][1], world.m[1][2] };
    const Vector3 z = { world.m[2][0], world.m[2][1], world.m[2][2] };
    box.size = { size.x * Vector3Length(x), size.y * Vector3Length(y), size.z * Vector3Length(z) };
    box.orientation[0] = NormalizeSafe(x);
    box.orientation[1] = NormalizeSafe(y);
    box.orientation[2] = NormalizeSafe(z);
    return box;
}

inline void BuildBell(const Matrix4x4& body, float pulse, std::vector<OBB>& boxes)
{
    // Follow the dome's surface with thin panels, never a solid bounding box.
    constexpr std::array<float, 7> radii = { 0.0f, 4.4f, 8.8f, 13.2f, 17.3f, 20.3f, 22.0f };
    constexpr std::array<float, 7> heights = { 12.0f, 11.7f, 10.9f, 9.4f, 7.2f, 4.3f, 0.0f };
    constexpr int kSlices = 32;
    const float radialScale = 1.0f + pulse * 0.018f;
    const float heightScale = 1.0f - pulse * 0.025f;
    const float halfAngle = std::numbers::pi_v<float> / kSlices;
    boxes.clear();
    boxes.reserve((radii.size() - 1) * kSlices + 16);
    for (size_t ring = 0; ring + 1 < radii.size(); ++ring) {
        const float innerRadius = radii[ring] * radialScale;
        const float outerRadius = radii[ring + 1] * radialScale;
        const float upper = heights[ring] * heightScale;
        const float lower = heights[ring + 1] * heightScale;
        const float dr = outerRadius - innerRadius;
        const float dy = lower - upper;
        const float length = std::sqrt(dr * dr + dy * dy);
        for (int slice = 0; slice < kSlices; ++slice) {
            const float angle = 2.0f * halfAngle * static_cast<float>(slice);
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const Vector3 tangent = { -s, 0.0f, c };
            const Vector3 meridian = { c * dr / length, dy / length, s * dr / length };
            const Vector3 normal = Cross(tangent, meridian);
            const Vector3 center = {
                c * (innerRadius + outerRadius) * 0.5f,
                (upper + lower) * 0.5f - 0.5f * heightScale,
                s * (innerRadius + outerRadius) * 0.5f
            };
            Matrix4x4 local = MatrixMath::MakeIdentity4x4();
            local.m[0][0] = tangent.x;
            local.m[0][1] = tangent.y;
            local.m[0][2] = tangent.z;
            local.m[1][0] = meridian.x;
            local.m[1][1] = meridian.y;
            local.m[1][2] = meridian.z;
            local.m[2][0] = normal.x;
            local.m[2][1] = normal.y;
            local.m[2][2] = normal.z;
            local.m[3][0] = center.x;
            local.m[3][1] = center.y;
            local.m[3][2] = center.z;
            boxes.push_back(TransformBox(MatrixMath::Multiply(local, body), {},
                { 2.0f * outerRadius * std::tan(halfAngle) + 0.12f, length + 0.3f, 1.65f }));
        }
    }
    for (int tooth = 0; tooth < 16; ++tooth) {
        const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(tooth) / 16.0f;
        const Matrix4x4 local = MatrixMath::MakeAffineMatrix(
            { 1.0f, 1.0f, 1.0f }, Vector3 { 0.0f, -angle, 0.0f },
            { std::cos(angle) * 21.45f * radialScale, -1.55f * heightScale,
              std::sin(angle) * 21.45f * radialScale });
        boxes.push_back(TransformBox(MatrixMath::Multiply(local, body), {},
            { 1.5f * radialScale, 3.1f * heightScale, 4.4f * radialScale }));
    }
}

inline SweepHit Sweep(const Sphere& bullet, const Vector3& movement,
    const std::vector<OBB>& bell, const std::vector<OBB>& tentacles,
    const Sphere& core, bool& hitCore)
{
    SweepHit nearest {};
    hitCore = false;
    for (const OBB& box : bell) {
        const SweepHit hit = CollisionManager::SweepSphere(bullet, movement, box);
        if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
            nearest = hit;
        }
    }
    for (const OBB& box : tentacles) {
        const SweepHit hit = CollisionManager::SweepSphere(bullet, movement, box);
        if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
            nearest = hit;
        }
    }
    const SweepHit coreHit = CollisionManager::SweepSphere(bullet, movement, core);
    if (coreHit.isHit && (!nearest.isHit || coreHit.time < nearest.time)) {
        nearest = coreHit;
        hitCore = true;
    }
    return nearest;
}
}
