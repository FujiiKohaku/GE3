#include "ShadowCamera.h"
#include "Engine/Camera/Camera.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cmath>

void ShadowCamera::Update(const Camera& camera, const Vector3& direction, const ShadowSettings& settings)
{
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
    Vector3 clip = MatrixMath::Transform(center, viewProjection_);
    return std::abs(clip.x) <= 1.0f + radius / extent_ &&
        std::abs(clip.y) <= 1.0f + radius / extent_ &&
        clip.z >= -radius / (2.0f * depthExtent_) &&
        clip.z <= 1.0f + radius / (2.0f * depthExtent_);
}
