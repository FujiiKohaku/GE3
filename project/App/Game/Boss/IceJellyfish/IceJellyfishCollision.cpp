#include "IceJellyfish.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include <numbers>

namespace {
// 調整用の設定値。意味が異なる値は、数値が同じでも個別に保持する。
constexpr float kBellRadius = 22.0f;
constexpr float kBellAnimationFrequency = 1.10f;
constexpr float kPrimaryWaveXFrequency = 0.16f;
constexpr float kPrimaryWaveZFrequency = 0.12f;
constexpr float kPrimaryWaveWeight = 0.65f;
constexpr float kSecondaryWavePhaseOffset = 0.45f;
constexpr float kSecondaryWaveXFrequency = 0.11f;
constexpr float kSecondaryWaveZFrequency = 0.18f;
constexpr float kSecondaryWaveWeight = 0.35f;
constexpr float kRadialPulseAmplitudeScale = 0.020f;
constexpr float kLegacyPulseRadialScale = 0.018f;
constexpr float kLegacyPulseHeightScale = 0.025f;
constexpr float kBellSurfaceHeightOffset = 0.5f;
constexpr float kPanelWidthPadding = 0.12f;
constexpr float kPanelLengthPadding = 0.3f;
constexpr float kPanelThickness = 1.65f;
constexpr float kRimToothRadius = 21.45f;
constexpr float kRimToothHeight = -1.55f;
constexpr Vector3 kRimToothSize = { 1.5f, 3.1f, 4.4f };

// Deformation.hlsliのDeformBellと同じ位置変形。法線の処理は判定では不要。
Vector3 DeformBellPosition(Vector3 position, const Vector4& animation)
{
    const float rimWeight = std::clamp((position.x * position.x + position.z * position.z) / (kBellRadius * kBellRadius), 0.0f, 1.0f);
    const float phase = animation.x * kBellAnimationFrequency + animation.z;
    const float wave = std::sin(phase + position.x * kPrimaryWaveXFrequency + position.z * kPrimaryWaveZFrequency) * kPrimaryWaveWeight +
        std::sin(phase - kSecondaryWavePhaseOffset - position.x * kSecondaryWaveXFrequency + position.z * kSecondaryWaveZFrequency) * kSecondaryWaveWeight;
    const float radialScale = 1.0f + animation.y * kRadialPulseAmplitudeScale * std::sin(animation.x * kBellAnimationFrequency);
    position.y += animation.y * rimWeight * wave;
    position.x *= radialScale;
    position.z *= radialScale;
    return position;
}

// 曲面を薄いOBBで近似する。角・辺・中心の27点を変形し、直交軸へ収める。
OBB MakeDeformedBellBox(const Matrix4x4& local, const Vector3& size,
    const Matrix4x4& world, const Vector4& animation)
{
    const auto transformPoint = [&](const Vector3& point) {
        return MatrixMath::Transform(DeformBellPosition(MatrixMath::Transform(point, local), animation), world);
    };
    const Vector3 center = transformPoint({});
    const Vector3 axisX = NormalizeSafe(transformPoint({ 0.5f, 0.0f, 0.0f }) - transformPoint({ -0.5f, 0.0f, 0.0f }));
    const Vector3 yDirection = transformPoint({ 0.0f, 0.5f, 0.0f }) - transformPoint({ 0.0f, -0.5f, 0.0f });
    const Vector3 axisY = NormalizeSafe(yDirection - axisX * Dot(axisX, yDirection));
    const Vector3 axisZ = NormalizeSafe(Cross(axisX, axisY));
    Vector3 minimum { (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() };
    Vector3 maximum = minimum * -1.0f;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                const Vector3 offset = transformPoint({ size.x * x * 0.5f, size.y * y * 0.5f, size.z * z * 0.5f }) - center;
                const Vector3 projected { Dot(offset, axisX), Dot(offset, axisY), Dot(offset, axisZ) };
                minimum = { (std::min)(minimum.x, projected.x), (std::min)(minimum.y, projected.y), (std::min)(minimum.z, projected.z) };
                maximum = { (std::max)(maximum.x, projected.x), (std::max)(maximum.y, projected.y), (std::max)(maximum.z, projected.z) };
            }
        }
    }
    const Vector3 midpoint = (minimum + maximum) * 0.5f;
    OBB box {};
    box.center = center + axisX * midpoint.x + axisY * midpoint.y + axisZ * midpoint.z;
    box.size = maximum - minimum;
    box.orientation[0] = axisX;
    box.orientation[1] = axisY;
    box.orientation[2] = axisZ;
    return box;
}
}

OBB IceJellyfish::MakeCollisionBox(const Matrix4x4& world, const Vector3& center, const Vector3& size)
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

void IceJellyfish::BuildBellColliders(const Matrix4x4& bellWorld, float pulse, std::vector<OBB>& boxes, const Vector4& animation)
{
    // Follow the dome's surface with thin panels, never a solid bounding box.
    constexpr std::array<float, 7> kRadii = { 0.0f, 4.4f, 8.8f, 13.2f, 17.3f, 20.3f, 22.0f };
    constexpr std::array<float, 7> kHeights = { 12.0f, 11.7f, 10.9f, 9.4f, 7.2f, 4.3f, 0.0f };
    constexpr int kSlices = 32;
    const float radialScale = 1.0f + pulse * kLegacyPulseRadialScale;
    const float heightScale = 1.0f - pulse * kLegacyPulseHeightScale;
    const float halfAngle = std::numbers::pi_v<float> / kSlices;
    boxes.clear();
    boxes.reserve((kRadii.size() - 1) * kSlices + 16);
    for (size_t ring = 0; ring + 1 < kRadii.size(); ++ring) {
        const float innerRadius = kRadii[ring] * radialScale;
        const float outerRadius = kRadii[ring + 1] * radialScale;
        const float upper = kHeights[ring] * heightScale;
        const float lower = kHeights[ring + 1] * heightScale;
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
                (upper + lower) * 0.5f - kBellSurfaceHeightOffset * heightScale,
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
            boxes.push_back(MakeDeformedBellBox(local,
                { 2.0f * outerRadius * std::tan(halfAngle) + kPanelWidthPadding, length + kPanelLengthPadding, kPanelThickness }, bellWorld, animation));
        }
    }
    for (int tooth = 0; tooth < 16; ++tooth) {
        const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(tooth) / 16.0f;
        const Matrix4x4 local = MatrixMath::MakeAffineMatrix(
            { 1.0f, 1.0f, 1.0f }, Vector3 { 0.0f, -angle, 0.0f },
            { std::cos(angle) * kRimToothRadius * radialScale, kRimToothHeight * heightScale,
              std::sin(angle) * kRimToothRadius * radialScale });
        boxes.push_back(MakeDeformedBellBox(local,
            { kRimToothSize.x * radialScale, kRimToothSize.y * heightScale, kRimToothSize.z * radialScale }, bellWorld, animation));
    }
}
