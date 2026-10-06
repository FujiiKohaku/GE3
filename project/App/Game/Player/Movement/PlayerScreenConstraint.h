#pragma once
#include "Engine/math/MathStruct.h"

class Camera;

class PlayerScreenConstraint {
public:
    PlayerScreenConstraint(const Vector3& basePosition, const Vector3& right,
        const Vector3& up, const Vector3& forward)
        : basePosition_(basePosition), right_(right), up_(up), forward_(forward) {}
    void SetCamera(Camera* camera) { camera_ = camera; }
    Vector3 ClampRailOffsetToScreen(const Vector3& railOffset) const;
    Vector3 CalculateRailWorldPosition(const Vector3& railOffset) const;

private:
    Vector2 CalculateScreenCorrection(const Vector3& railOffset) const;
    void UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX,
        float& minY, float& maxY) const;
    const Vector3& basePosition_;
    const Vector3& right_;
    const Vector3& up_;
    const Vector3& forward_;
    Camera* camera_ = nullptr;
    float railMoveLimitX_ = 20.0f;
    float railMoveLimitY_ = 12.0f;
    float playerClampMarginX_ = 100.0f;
    float playerClampMarginY_ = 100.0f;
    float playerBoundsHalfWidth_ = 1.5f;
    float playerBoundsHalfHeight_ = 1.0f;
};
