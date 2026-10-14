#pragma once
#include "Engine/math/MathStruct.h"

class Camera;
struct Ray;

class PlayerAimController {
public:
    // 使用クラス：Player（Initialize）。照準位置と照準距離を初期化する。
    void Initialize();
    // 使用クラス：Player（Update）。マウス位置を照準に反映する。
    void UpdateMouseAim();
    void UpdateGamepadAim(float stickX, float stickY, bool invertedPitch, float deltaTimeSeconds, float sensitivity = 1.0f);
    // 使用クラス：Player（Update）。照準位置を画面内に制限する。
    void ClampAimScreenPosition();
    // パッドの照準を、機体の画面位置から短辺の30%以内に制限する。
    void ClampGamepadAimToPlayer(const Vector2& playerScreenPosition);
    // 機体から照準への方向を、Steeringが受け取る中央基準の座標へ変換する。
    Vector2 GetGamepadSteeringScreenPosition() const;
    // 使用クラス：PlayerWeaponController（発射・デバッグ描画）、本クラス（照準距離の更新）。
    void CreateAimRay(Ray& aimRay, const Camera& activeCamera) const;
    // 現在は呼び出し元なし。基準距離にある収束点を返す。
    Vector3 CreateConvergencePoint(const Ray& aimRay) const;
    // 使用クラス：Player（Update）。狙う対象までの距離を補間する。
    void UpdateSmoothedAimDistance(const Camera& activeCamera, float deltaTimeSeconds);
    // 使用クラス：PlayerWeaponController（発射・デバッグ描画）。補間済みの狙い位置を返す。
    Vector3 ResolveAimPoint(const Ray& aimRay) const;
    // 使用クラス：Player（外部への公開）、PlayerMovementController（操舵）、PlayerWeaponController（ロックオン）。
    const Vector2& GetAimScreenPosition() const { return aimScreenPosition_; }

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
