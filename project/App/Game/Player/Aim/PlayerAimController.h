#pragma once
#include "Engine/math/MathStruct.h"

class Camera;
struct Ray;

// 照準の画面位置、入力による移動、射撃先の距離を管理する。
class PlayerAimController {
public:
    void InitializeAim();
    void UpdateMouseAim();
    void UpdateGamepadAim(float stickX, float stickY, bool isPitchInverted, float deltaTimeSeconds, float sensitivity = 1.0f);
    void ClampAimScreenPosition();
    void ClampGamepadAimToPlayer(const Vector2& playerScreenPosition);
    Vector2 GetGamepadSteeringScreenPosition() const;
    const Vector2& GetAimScreenPosition() const { return aimScreenPosition_; }
    void CreateAimRay(Ray& aimRay, const Camera& activeCamera) const;
    Vector3 CreateConvergencePoint(const Ray& aimRay) const;
    void UpdateSmoothedAimDistance(const Camera& activeCamera, float deltaTimeSeconds);
    Vector3 ResolveAimPoint(const Ray& aimRay) const;

private:
    static constexpr float kGamepadMaxAimDistanceRatio = 0.40f;
    const float kAimConvergenceDistance = 220.0f;
    const float kAimDistanceFollowSpeed = 12.0f;
    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };
    Vector2 gamepadAimOffset_ = {};
    Vector2 gamepadAimAnchor_ = {};
    bool hasGamepadAimAnchor_ = false;
    float smoothedAimDistance_ = kAimConvergenceDistance;
};
