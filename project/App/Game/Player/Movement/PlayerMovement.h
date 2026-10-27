#pragma once
#include "Engine/math/MathStruct.h"
#include "App/Game/Stage/RailHeightRange.h"

class Camera;

// 操舵・飛行・レール上の自機位置を管理する。PlayerとStateには依存しない。
class PlayerMovement {
public:
    void SetCamera(const Camera* camera) { camera_ = camera; }
    const Vector3& GetRailOffset() const { return railOffset_; }
    const Vector3& GetFlightForward() const { return railForward_; }
    bool IsAllRangeMode() const { return isAllRangeMode_; }
    bool IsReturningToFlightArea() const { return isReturningToFlightArea_; }
    bool IsBoosting() const { return isBoosting_; }
    bool IsRolling() const { return isRolling_; }
    float GetForwardSpeedMultiplier() const { return velocity_.z / normalMaxSpeed_; }
    float GetManualBank() const { return manualBank_; }
    float GetMouseSensitivity() const { return mouseSensitivity_; }
    const Vector2& GetSteeringInput() const { return steeringInput_; }
    Vector3 GetWorldVelocity() const { return railForward_ * velocity_.z; }
    void SetRailMinimumHeight(float height) { railMinimumHeight_ = height; hasRailMinimumHeight_ = true; hasRailHeightRange_ = false; }
    void SetRailHeightRange(const RailHeightRange& range) { railHeightRange_ = range; hasRailHeightRange_ = true; }
    void ResetSteering() { steeringInput_ = {}; }
    void StopRoll() { isRolling_ = false; }
    void UpdateMovement(EulerTransform& transform, const Vector2& steering, float railResponse,
        float manualBank, bool isLeftTriggered, bool isRightTriggered, float deltaTimeSeconds);
    void InitializeMovement(const EulerTransform& transform);
    void SynchronizePosition(const EulerTransform& transform);
    void SynchronizeOrientation(const EulerTransform& transform);
    void ApplyRailPosition(EulerTransform& transform);
    void StopMovement();
    Vector3 GetAutomaticMovementVelocity() const;
    void SetRailFrame(const Vector3& railBasePosition,
        const Vector3& railRight, const Vector3& railUp, const Vector3& railForward);
    void ApplyRailAreaForce(const Vector3& force, float deltaTimeSeconds);
    // ブーストを開始したフレームだけtrueを返す。効果音は呼び出し側で再生する。
    bool UpdateBoost(bool isBraking, bool shouldBoost);
    void EnableAllRangeMode(const EulerTransform& transform, float areaRadius, float minHeight, float maxHeight);
    void SetMouseSensitivity(float sensitivity);
    void UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
        bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad);
    void UpdateRoll(EulerTransform& transform, bool isLeftTriggered, bool isRightTriggered, float deltaTimeSeconds);
    Vector3 CalculateRailWorldPosition(const Vector3& railOffset) const;
    Vector3 ClampRailOffsetToScreen(const Vector3& railOffset) const;

private:
    void UpdateAllRangeMove(EulerTransform& transform, Vector2 steering, float deltaTimeSeconds);
    void UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds);
    void UpdateFlightAreaReturn(const EulerTransform& transform, Vector2& steering, float deltaTimeSeconds);
    void UpdateFlightHeight(const EulerTransform& transform, float deltaTimeSeconds);
    void UpdateFlightPosition(EulerTransform& transform, float deltaTimeSeconds);
    void UpdateFlightBank(EulerTransform& transform, const Vector2& steering, float deltaTimeSeconds);
    Vector2 CalculateScreenCorrection(const Vector3& railOffset) const;
    void UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const;
    void EnforceHeightRange(Vector3& offset) const;
    // カメラは外部所有。Playerから設定された参照を利用する。
    const Camera* camera_ = nullptr;

    bool isBoosting_ = false;
    bool isAllRangeMode_ = false;
    bool isReturningToFlightArea_ = false;
    float flightAreaRadius_ = 600.0f;
    float flightMinHeight_ = 5.0f;
    float flightMaxHeight_ = 180.0f;
    float flightYaw_ = 0.0f;
    float flightPitch_ = 0.0f;
    float normalMaxSpeed_ = 0.5f;
    float boostMaxSpeed_ = 1.0f;
    float normalAcceleration_ = 0.2f;
    float boostAcceleration_ = 0.35f;
    float moveSpeed_ = normalAcceleration_;
    Vector3 velocity_ = { 0.0f, 0.0f, 0.5f };
    Vector3 railBasePosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 railRight_ = { 1.0f, 0.0f, 0.0f };
    Vector3 railUp_ = { 0.0f, 1.0f, 0.0f };
    Vector3 railForward_ = { 0.0f, 0.0f, 1.0f };
    Vector3 railOffset_ = { 0.0f, 0.0f, 0.0f };
    float manualBank_ = 0.0f;

    float mouseSensitivity_ = 1.0f; // 中央からの照準のずれに掛ける倍率。
    Vector2 steeringInput_ = {}; // 補間済みの入力。Xは右向き、Yは上向きが正で、中央は0。

    bool isRolling_ = false;
    float rollTimerSeconds_ = 0; // ロール開始からの経過秒数。
    float rollCooldownSeconds_ = 0;
    float rollDirection_ = 0.0f; // 左入力は+1、右入力は-1で、回転方向を切り替える。
    float leftKeyTapTimerSeconds_ = 0;
    float rightKeyTapTimerSeconds_ = 0;
    // 60FPSでの従来の長さを、経過秒数で管理する。
    static constexpr float kMaxTapIntervalSeconds = 0.25f;
    static constexpr float kRollDurationSeconds = 0.5f;
    static constexpr float kRollCooldownDurationSeconds = 1.0f;

    float railMoveLimitX_ = 20.0f;
    float railMoveLimitY_ = 15.0f;
    float railMinimumHeight_ = 0.0f;
    bool hasRailMinimumHeight_ = false;
    RailHeightRange railHeightRange_;
    bool hasRailHeightRange_ = false;
    // 画面端に残す余白。単位はピクセル。
    float playerClampMarginX_ = 10.0f;
    float playerClampMarginY_ = 10.0f;
    // 補正判定に使う機体の半幅・半高さ。単位はワールド座標。
    float playerBoundsHalfWidth_ = 1.5f;
    float playerBoundsHalfHeight_ = 1.0f;

};
