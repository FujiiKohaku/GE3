#pragma once
#include "Engine/math/MathStruct.h"
#include "App/Game/Player/Aim/PlayerAimController.h"
#include "ScreenConstraint/PlayerScreenConstraint.h"
#include "Roll/PlayerRollController.h"
#include "Steering/PlayerSteeringController.h"

class Camera;
class Input;

class PlayerMovementController {
public:
    enum class ControlMode { KeyboardAndMouse, StarFox };
    // 使用クラス：Player（メンバー初期化）。機体の姿勢を更新し、Aimの照準位置を参照する。
    PlayerMovementController(EulerTransform& transform, const PlayerAimController& aimController): transform_(transform), aimController_(aimController) {}
    // 自身のレール座標系をScreenConstraintが参照するため、コピー・ムーブを禁止する。
    PlayerMovementController(const PlayerMovementController&) = delete;
    PlayerMovementController& operator=(const PlayerMovementController&) = delete;
    PlayerMovementController(PlayerMovementController&&) = delete;
    PlayerMovementController& operator=(PlayerMovementController&&) = delete;
    // 使用クラス：Player（Initialize）。現在位置をレールの基準位置にする。
    void Initialize();
    // 使用クラス：Player（SetTranslate・SetRotate）。外部からの位置・姿勢変更を移動状態へ同期する。
    void SynchronizePosition();
    void SynchronizeOrientation();
    // 使用クラス：Player（SetCamera）。画面内補正にカメラを渡す。
    void SetCamera(Camera* camera) { screenConstraint_.SetCamera(camera); }
    // 使用クラス：Player（Update）。ブースト入力から速度と移動量を切り替える。
    void UpdateBoost(Input* input, bool isDebugMode);
    // 使用クラス：Player（Update）。ロールを更新し、モードに応じた移動を実行する。
    void UpdateMovement(Input* input);
    // 使用クラス：Player（Update）。レール上の移動量を機体のワールド位置に反映する。
    void ApplyRailPosition();
    // 使用クラス：Player（死亡時）。ブーストとロールの状態を解除する。
    void Stop();
    // 使用クラス：Player（EnableAllRangeMode）。飛行範囲を設定し、全方向飛行へ切り替える。
    void EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight);
    // 使用クラス：Player（外部への公開）。全方向飛行中かを返す。
    bool IsAllRangeMode() const { return isAllRangeMode_; }
    // 使用クラス：Player（外部への公開）。飛行範囲への自動復帰中かを返す。
    bool IsReturningToFlightArea() const { return isReturningToFlightArea_; }
    // 使用クラス：Player（外部への公開）。ブースト中かを返す。
    bool IsBoosting() const { return isBoosting_; }
    // 使用クラス：Player（被弾判定・外部への公開）。ロール中かを返す。
    bool IsRolling() const { return rollController_.IsRolling(); }
    // 使用クラス：Player・PlayerWeaponController。飛行の前方向を返す。
    const Vector3& GetFlightForward() const { return railForward_; }
    // 使用クラス：Player（外部への公開）。レール基準位置からの移動量を返す。
    const Vector3& GetRailOffset() const { return railOffset_; }
    // 使用クラス：PlayerWeaponController（弾の速度計算）。ブーストを含む前進速度を返す。
    Vector3 GetWorldVelocity() const { return railForward_ * velocity_.z; }
    // 使用クラス：Player（外部への公開）。レール飛行では通常速度、全方向飛行では現在速度を返す。
    Vector3 GetAutomaticWorldVelocity() const;
    // 使用クラス：Player（SetRailFrame）。ステージ側で求めたレールの基準位置と方向を受け取る。
    void SetRailFrame(const Vector3& railBasePosition, const Vector3& railRight,
        const Vector3& railUp, const Vector3& railForward);
    // 使用クラス：Player（ApplyRailAreaForce）。外力を左右・上下の移動量へ足し、画面内に補正する。
    void ApplyRailAreaForce(const Vector3& force);
    // 使用クラス：Player（SetControlMode）。操作方式を切り替え、前の操舵入力を消す。
    void SetControlMode(ControlMode mode);
    // 使用クラス：Player（外部への公開）。現在の操作方式を返す。
    ControlMode GetControlMode() const { return controlMode_; }
    // 使用クラス：Player（外部への公開）。Steeringへマウス感度を設定する。
    void SetMouseSensitivity(float sensitivity) { steeringController_.SetMouseSensitivity(sensitivity); }
    // 使用クラス：Player（外部への公開）。Steeringのマウス感度を返す。
    float GetMouseSensitivity() const { return steeringController_.GetMouseSensitivity(); }
    // 使用クラス：Player（外部への公開）。補間済みのマウス操舵入力を返す。
    const Vector2& GetStarFoxSteeringInput() const { return steeringController_.GetSteeringInput(); }

private:
    // 使用クラス：本クラス（UpdateMovement）。WASDでレール上の左右・上下移動を更新する。
    void UpdateKeyboardMove(Input* input);
    // 使用クラス：本クラス（UpdateMovement）。マウス操舵でレール上の移動を更新する。
    void UpdateStarFoxMove();
    // 使用クラス：本クラス（レール移動・全方向飛行）。Aimの照準位置からSteeringの入力を更新する。
    void UpdateStarFoxSteering();
    // 使用クラス：本クラス（UpdateMovement）。旋回・範囲制限・前進・機体の傾きを順に更新する。
    void UpdateAllRangeMove(Input* input);
    // 使用クラス：本クラス（UpdateAllRangeMove）。操舵に応じてヨーとピッチを更新する。
    void UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds);
    // 使用クラス：本クラス（UpdateAllRangeMove）。飛行範囲の端で内側へ自動旋回させる。
    void UpdateFlightAreaReturn(Vector2& steering, float deltaTimeSeconds);
    // 使用クラス：本クラス（UpdateAllRangeMove）。高度の上下限付近でピッチを戻す。
    void UpdateFlightHeight(float deltaTimeSeconds);
    // 使用クラス：本クラス（UpdateAllRangeMove）。姿勢から飛行方向を求め、位置を更新する。
    void UpdateFlightPosition(float deltaTimeSeconds);
    // 使用クラス：本クラス（UpdateAllRangeMove）。ロール中以外は旋回に合わせて機体を傾ける。
    void UpdateFlightBank(const Vector2& steering, float deltaTimeSeconds);

    EulerTransform& transform_;
    const PlayerAimController& aimController_;
    bool isBoosting_ = false;
    ControlMode controlMode_ = ControlMode::StarFox;
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
    // 細かな画面内補正・ロール・操舵計算は、各部品へ委譲する。
    PlayerScreenConstraint screenConstraint_{ railBasePosition_, railRight_, railUp_, railForward_ };
    PlayerRollController rollController_;
    PlayerSteeringController steeringController_;
};
