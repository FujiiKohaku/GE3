#pragma once
#include "ShadowSettings.h"
#include <array>
class Camera;

class ShadowCamera {
public:
    void Update(const Camera& camera, const Vector3& direction, const ShadowSettings& settings);
    void UpdatePerspective(const Vector3& position, const Vector3& direction, float distance, float fovY);
    bool Intersects(const Vector3& center, float radius) const;
    const Matrix4x4& GetViewProjection() const { return viewProjection_; }
private:
    std::array<Vector4, 6> frustumPlanes_ {};
    bool isPerspective_ = false;
    Vector3 lightPosition_ {};
    float lightDistance_ = 1.0f;
    Matrix4x4 viewProjection_ {};
    float extent_ = 1.0f;
    float depthExtent_ = 1.0f;
};
