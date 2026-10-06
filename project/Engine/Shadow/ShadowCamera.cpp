#include "ShadowCamera.h"
#include "Engine/Camera/Camera.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cmath>

void ShadowCamera::Update(const Camera& camera, const Vector3& direction, const ShadowSettings& settings)
{
    isPerspective_ = false;
    float distance = (std::min)(settings.distance, camera.GetFarClip());
    distance = (std::max)(distance, camera.GetNearClip() + 1.0f);
    float heights[2] = { camera.GetNearClip(), distance };
    Vector3 corners[8] {};
    Vector3 center {};
    int index = 0;
    for (float z : heights) {
        float halfHeight = std::tan(camera.GetFovY() * 0.5f) * z;
        float halfWidth = halfHeight * camera.GetAspectRatio();
        for (int y = -1; y <= 1; y += 2) {
            for (int x = -1; x <= 1; x += 2) {
                corners[index] = MatrixMath::Transform({ halfWidth * x, halfHeight * y, z }, camera.GetWorldMatrix());
                center += corners[index];
                ++index;
            }
        }
    }
    center = center * (1.0f / 8.0f);
    float radius = 0.0f;
    for (const Vector3& corner : corners) {
        radius = (std::max)(radius, Vector3Length(corner - center));
    }
    // A fixed sphere avoids changing the shadow scale when the camera rotates.
    extent_ = std::ceil((radius + 16.0f) / 16.0f) * 16.0f;
    depthExtent_ = extent_ + settings.casterMargin;
    Vector3 forward = NormalizeSafe(direction);
    if (IsNearlyZero(forward)) { forward = { 0.0f, -1.0f, 0.0f }; }
    Vector3 reference { 0.0f, 1.0f, 0.0f };
    if (std::abs(Dot(reference, forward)) > 0.95f) { reference = { 0.0f, 0.0f, 1.0f }; }
    Vector3 right = Normalize(Cross(reference, forward));
    Vector3 up = Cross(forward, right);
    float texel = extent_ * 2.0f / static_cast<float>(settings.resolution);
    float centerX = std::round(Dot(center, right) / texel) * texel;
    float centerY = std::round(Dot(center, up) / texel) * texel;
    Matrix4x4 view = MatrixMath::MakeIdentity4x4();
    view.m[0][0] = right.x; view.m[1][0] = right.y; view.m[2][0] = right.z;
    view.m[0][1] = up.x; view.m[1][1] = up.y; view.m[2][1] = up.z;
    view.m[0][2] = forward.x; view.m[1][2] = forward.y; view.m[2][2] = forward.z;
    view.m[3][0] = -centerX; view.m[3][1] = -centerY; view.m[3][2] = -Dot(center, forward);
    Matrix4x4 projection = MatrixMath::MakeOrthographicMatrix(-extent_, extent_, extent_, -extent_, -depthExtent_, depthExtent_);
    viewProjection_ = MatrixMath::Multiply(view, projection);
}

bool ShadowCamera::Intersects(const Vector3& center, float radius) const
{
    if (isPerspective_) {
        if (Vector3Length(center - lightPosition_) > lightDistance_ + radius) { return false; }
        for (const Vector4& plane : frustumPlanes_) {
            if (plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w < -radius) { return false; }
        }
        return true;
    }
    Vector3 clip = MatrixMath::Transform(center, viewProjection_);
    return std::abs(clip.x) <= 1.0f + radius / extent_ &&
        std::abs(clip.y) <= 1.0f + radius / extent_ &&
        clip.z >= -radius / (2.0f * depthExtent_) &&
        clip.z <= 1.0f + radius / (2.0f * depthExtent_);
}

void ShadowCamera::UpdatePerspective(const Vector3& position, const Vector3& direction, float distance, float fovY)
{
    isPerspective_ = true;
    lightPosition_ = position;
    lightDistance_ = distance;
    const Vector3 forward = NormalizeSafe(direction);
    Vector3 reference { 0.0f, 1.0f, 0.0f };
    if (std::abs(Dot(reference, forward)) > 0.95f) { reference = { 0.0f, 0.0f, 1.0f }; }
    const Vector3 right = NormalizeSafe(Cross(reference, forward));
    const Vector3 up = Cross(forward, right);
    Matrix4x4 view = MatrixMath::MakeIdentity4x4();
    view.m[0][0] = right.x; view.m[1][0] = right.y; view.m[2][0] = right.z;
    view.m[0][1] = up.x; view.m[1][1] = up.y; view.m[2][1] = up.z;
    view.m[0][2] = forward.x; view.m[1][2] = forward.y; view.m[2][2] = forward.z;
    view.m[3][0] = -Dot(position, right); view.m[3][1] = -Dot(position, up); view.m[3][2] = -Dot(position, forward);
    viewProjection_ = MatrixMath::Multiply(view, MatrixMath::MakePerspectiveFovMatrix(fovY, 1.0f, 0.1f, distance));
    // 球の境界と各面の視錐台で絞り、ポイントライト6面の重複描画を減らす。
    const auto& matrix = viewProjection_.m;
    for (uint32_t planeIndex = 0; planeIndex < 6; ++planeIndex) {
        const uint32_t axis = planeIndex / 2;
        float sign = 1.0f;
        if (planeIndex % 2 != 0) { sign = -1.0f; }
        Vector4 plane { matrix[0][3] + sign * matrix[0][axis], matrix[1][3] + sign * matrix[1][axis],
            matrix[2][3] + sign * matrix[2][axis], matrix[3][3] + sign * matrix[3][axis] };
        if (planeIndex == 4) { plane = { matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2] }; }
        const float inverseLength = 1.0f / std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        frustumPlanes_[planeIndex] = { plane.x * inverseLength, plane.y * inverseLength, plane.z * inverseLength, plane.w * inverseLength };
    }
}
