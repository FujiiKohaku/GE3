#pragma once
#include "ShadowSettings.h"
class Camera;

class ShadowCamera {
public:
    void Update(const Camera& camera, const Vector3& direction, const ShadowSettings& settings);
    bool Intersects(const Vector3& center, float radius) const;
    const Matrix4x4& GetViewProjection() const { return viewProjection_; }
private:
    Matrix4x4 viewProjection_ {};
    float extent_ = 1.0f;
    float depthExtent_ = 1.0f;
};
