#pragma once
#include "Engine/math/MathStruct.h"

// PlayerReticleController
class Camera;

// レティクルの画面位置、入力による移動、表示範囲の制限を管理する。
class PlayerReticleController {
public:
    static constexpr Vector2 kReticleSizeUi = { 128.0f, 128.0f };
    void InitializeAim();
    void UpdateMouseAim();
    void UpdateGamepadAim(float stickX, float stickY, bool isPitchInverted, float deltaTimeSeconds, float sensitivity = 1.0f);
    void ClampAimScreenPosition();
    void ClampGamepadAimToPlayer(const Vector2& playerScreenPosition);
    Vector2 GetGamepadSteeringScreenPosition() const;
    const Vector2& GetAimScreenPosition() const { return aimScreenPosition_; }
    void UpdateFromWorldPoint(const Camera& activeCamera, const Vector3& worldPoint);

private:
    static constexpr float kGamepadMaxAimDistanceRatio = 0.40f;
    Vector2 aimScreenPosition_ = { 0.0f, 0.0f };
    Vector2 gamepadAimOffset_ = {};
    Vector2 gamepadAimAnchor_ = {};
    bool hasGamepadAimAnchor_ = false;
};
