// Playerの姿勢と公開APIを保持し、登場・戦闘・死亡の更新を各Stateへ委譲する。
#include "Player.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Camera/Camera.h"
#include <cassert>
#include <cmath>
#include "Engine/Winapp/WinApp.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include <algorithm>
#include "Engine/audio/SoundManager.h"

// 機体のモデルと武器を準備し、照準と移動の初期状態を揃える。
void Player::Initialize(Model* model)
{
    assert(model != nullptr);

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetMaterial("resources/Shaders/Object3D/ShadowStandard");
    object_->SetNormalMap("resources/Textures/Normals/metal_detail.png", 0.25f);
    object_->SetCastShadow(true);
    object_->SetReceiveShadow(true);
    object_->SetEnableLighting(true);
    object_->SetModel(model);
    object_->SetSurfaceProperties(0.30f, 0.72f, 0.90f);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }

    weaponController_.Initialize();

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, -2.0f, 0.0f };
    InitializeMovement();

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    // プレイヤー初期化時に、照準を画面中央へ置き、狙う距離を基準値に戻す。
    InitializeAim();
}

// 入力・被弾状態・移動・武器を更新し、最後に機体の姿勢を描画へ反映する。
void Player::Update()
{
    if (object_ == nullptr || TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        // 未初期化・時間停止中は、入力や弾を含めて更新を進めない。
        return;
    }
    if (state_ == State::Entry) {
        entryState_.Update(*this);
    }
    switch (state_) {
    case State::Battle:
        battleState_.Update(*this);
        break;
    case State::Dead:
        deadState_.Update(*this);
        break;
    case State::Entry:
        break;
    }
}

// レールを進める前に加減速入力を読み取り、このフレームの速度を確定する。
void Player::UpdateBoostState()
{
    if (!IsAlive()) {
        StopMovement();
        return;
    }
    if (object_ == nullptr || TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        return;
    }
    Input* input = Input::GetInstance();
    if (input == nullptr) {
        return;
    }
    if (debugCameraController_ != nullptr) {
        isDebugMode_ = debugCameraController_->GetDebugMode();
    }
    UpdateBoost(input, isDebugMode_);
}

void Player::SetCamera(Camera* camera)
{
    camera_ = camera;
    movement_.SetCamera(camera);
    weaponController_.SetCamera(camera);
    if (object_ != nullptr) {
        object_->SetCamera(camera);
    }
}

void Player::SetDebugCameraController(DebugCameraController* debugCameraController)
{
    debugCameraController_ = debugCameraController;
}

void Player::SetEnableLighting(bool isEnabled)
{
    if (object_ != nullptr) {
        object_->SetEnableLighting(isEnabled);
    }
}

void Player::SetTranslate(const Vector3& translate)
{
    transform_.translate = translate;
    // 次のレール位置計算で、外部から設定した位置が元へ戻らないようにする。
    SynchronizePosition();
    if (object_ != nullptr) {
        ApplyTransform();
        object_->Update();
    }
}

void Player::SetRotate(const Vector3& rotate)
{
    transform_.rotate.x = rotate.x;
    transform_.rotate.y = rotate.y;
    transform_.rotate.z = rotate.z;
    SynchronizeOrientation();
}

Vector3 Player::GetAutomaticWorldVelocity() const
{
    if (IsDead()) {
        return {};
    }
    return GetAutomaticMovementVelocity();
}

// HP側の被弾判定を行い、死亡した場合は移動とロックオンも停止する。
bool Player::ApplyDamage(int damage)
{
    const bool hasAppliedDamage = ApplyHealthDamage(damage);
    if (hasAppliedDamage && IsDead()) {
        // 死亡状態への移行に合わせ、ブースト・ロックオンを解除する。
        ChangeState(State::Dead);
    }
    return hasAppliedDamage;
}

// 現在表示するモデルや追加パーツを描画する。
void Player::Draw()
{
    if (object_ == nullptr) {
        return;
    }
    weaponController_.DrawBullets();
    // 被弾後の点滅で機体を隠す場合も、発射済みの弾は表示する。
    if (ShouldDraw()) {
        object_->Draw();
    }
}

// 影を落とす対象のモデルを、影用の描画処理へ渡す。
void Player::DrawShadow(ShadowMapRenderer& renderer)
{
    if (object_ != nullptr && ShouldDraw()) {
        object_->DrawShadow(renderer);
    }
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void Player::SubmitRaytracing(DxrRenderer& renderer) {
    if (!object_) { return; }
    weaponController_.SubmitRaytracing(renderer);
    if (ShouldDraw()) { object_->SubmitRaytracing(renderer); }
}

// 機体の位置・回転・拡大率を描画オブジェクトへ反映する。
void Player::ApplyTransform()
{
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

// 開発用の画面に内部状態や調整項目を表示する。
void Player::DrawImGui()
{
#ifdef _DEBUG
    ImGui::Begin("Player Controls");
    ImGui::Text("HP: %d / %d", GetCurrentHp(), GetMaxHp());
    ImGui::Text("Weapon: %s", weaponController_.GetCurrentWeaponName());
    ImGui::Text("Heat: %.2f", weaponController_.GetHeatRatio());
    ImGui::End();
#endif
}
Vector3 Player::GetEngineExhaustPosition() const
{
    const Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(
        transform_.scale, transform_.rotate, transform_.translate);
    return MatrixMath::Transform(Vector3 { 0.0f, 0.0f, -1.0f }, worldMatrix);
}

Vector3 Player::GetEngineExhaustDirection() const
{
    const Matrix4x4 rotationMatrix = MatrixMath::MakeAffineMatrix(
        Vector3 { 1.0f, 1.0f, 1.0f }, transform_.rotate, Vector3 {});
    return Normalize(MatrixMath::Transform(Vector3 { 0.0f, 0.0f, -1.0f }, rotationMatrix));
}

// 使用中のパッドを優先し、パッドでなければ設定された操作方式を選ぶ。
Player::ControlInputType Player::GetControlInputType(const Input& input) const
{
    return PlayerInput::GetControlInputType(input, controlMode_ == ControlMode::Controller);
}

// 選択された操作方式に合わせて、マウスまたはパッドの照準を更新する。
void Player::UpdateAimFromInput(const Input& input, float deltaTimeSeconds)
{
    if (GetControlInputType(input) == ControlInputType::Gamepad) {
        const PlayerInput::AimInput aimInput = PlayerInput::ReadGamepadAim(input);
        UpdateGamepadAim(aimInput.stick.x, aimInput.stick.y,
            aimInput.isPitchInverted, deltaTimeSeconds, aimInput.sensitivity);
    } else {
        UpdateMouseAim();
    }
}

// 照準の画面位置を機体の左右・上下の操舵入力へ変換する。
Player::ControlInput Player::UpdateAimSteering(const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad)
{
    Vector2 aimPosition = GetAimScreenPosition();
    if (isGamepad) { aimPosition = GetGamepadSteeringScreenPosition(); }
    UpdateSteering(aimPosition, screenSizePixels, isAllRangeMode, deltaTimeSeconds, isGamepad);
    return { movement_.GetSteeringInput(), 2.75f };
}

// キーボード・マウス・パッドの違いを、移動処理が共通に扱える入力へまとめる。
Player::ControlInput Player::GetControlInput(const Input& input, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds)
{
    const ControlInputType inputType = GetControlInputType(input);
    if (inputType == ControlInputType::Gamepad) {
        return UpdateAimSteering(screenSizePixels, isAllRangeMode, deltaTimeSeconds, true);
    }
    if (inputType == ControlInputType::Mouse) {
        return UpdateAimSteering(screenSizePixels, isAllRangeMode, deltaTimeSeconds, false);
    }
    if (isAllRangeMode) { UpdateAimSteering(screenSizePixels, true, deltaTimeSeconds, false); }
    return { PlayerInput::ReadKeyboardSteering(input), 1.0f };
}

// 操作できる状態かを確認してから、機体の画面位置を基準にパッド照準を補正する。
void Player::ConstrainGamepadAimToPlayer()
{
    if (isDebugMode_ || !IsAlive()) { return; }
    const Input* input = Input::GetInstance();
    if (input == nullptr || GetControlInputType(*input) != ControlInputType::Gamepad || camera_ == nullptr) { return; }
    const Vector3 viewPosition = MatrixMath::Transform(transform_.translate, camera_->GetViewMatrix());
    if (!std::isfinite(viewPosition.z) || viewPosition.z <= camera_->GetNearClip()) { return; }
    ClampGamepadAimToPlayer(camera_->WorldToScreen(transform_.translate));
}

// 被弾後の無敵時間を経過秒数で進める。残り時間が0になると通常状態に戻る。
void Player::UpdateInvincibility(float deltaTimeSeconds)
{
    if (invincibleRemainingSeconds_ > 0.000001f) {
        invincibleRemainingSeconds_ = (std::max)(0.0f, invincibleRemainingSeconds_ - deltaTimeSeconds);
    }
}

// 無敵中の点滅と死亡状態に応じて描画を判定する。
bool Player::ShouldDraw() const
{
    if (state_ == State::Dead) {
        return deadState_.ShouldDraw();
    }
    return invincibleRemainingSeconds_ <= 0.000001f || static_cast<int>(invincibleRemainingSeconds_ * 15.0f + 0.00001f) % 2 == 0;
}

// 無敵時間を確認し、HPを減らす。死亡状態への遷移は呼び出し元で行う。
bool Player::ApplyHealthDamage(int damage)
{
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (isInvincibleMode_) {
        return false;
    }
#endif
    if (damage <= 0 || invincibleRemainingSeconds_ > 0.000001f || currentHp_ <= 0) {
        return false;
    }

    currentHp_ -= damage;
    SoundManager::GetInstance()->Play("PlayerDamage");
    if (currentHp_ < 0) {
        currentHp_ = 0;
    }
    invincibleRemainingSeconds_ = kInvincibleDurationSeconds;
    return true;
}
// 生存中のHPを上限まで回復する。回復量が0以下、死亡中、またはHPが上限に達している場合は回復しない。
bool Player::Heal(int amount)
{
    if (amount <= 0 || currentHp_ <= 0 || currentHp_ >= maxHp_) {
        return false;
    }

    currentHp_ += amount;
    SoundManager::GetInstance()->Play("HealPickup");
    if (currentHp_ > maxHp_) {
        currentHp_ = maxHp_;
    }

    return true;
}
// 初期位置と姿勢を移動用の内部状態へ揃える。
void Player::InitializeMovement()
{
    movement_.InitializeMovement(transform_);
}

// 外部から設定された機体位置を、レール基準の移動状態へ反映する。
void Player::SynchronizePosition()
{
    movement_.SynchronizePosition(transform_);
}

// 外部から設定された回転角を、全方向飛行の機首方向へ反映する。
void Player::SynchronizeOrientation()
{
    movement_.SynchronizeOrientation(transform_);
}

// 操作入力を読み、レール飛行か全方向飛行かに応じて移動処理を選ぶ。
void Player::UpdateMovement(Input* input)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    const ControlInput control = GetControlInput(*input,
        { static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
          static_cast<float>(WinApp::GetInstance()->GetClientHeight()) },
        movement_.IsAllRangeMode(), deltaTimeSeconds);
    movement_.UpdateMovement(transform_, control.steering, control.railResponse, deltaTimeSeconds);
}

// レール基準位置と操縦によるずれを合成し、画面内に収まる機体位置を決める。
void Player::ApplyRailPosition()
{
    movement_.ApplyRailPosition(transform_);
}

// 死亡や操作停止時に、速度とロールなどの移動状態を解除する。
void Player::StopMovement()
{
    movement_.StopMovement();
}

Vector3 Player::GetAutomaticMovementVelocity() const
{
    return movement_.GetAutomaticMovementVelocity();
}

void Player::SetRailFrame(const Vector3& railBasePosition,
    const Vector3& railRight, const Vector3& railUp, const Vector3& railForward)
{
    movement_.SetRailFrame(railBasePosition, railRight, railUp, railForward);
}

void Player::ApplyRailAreaForce(const Vector3& force)
{
    movement_.ApplyRailAreaForce(force, TimeManager::GetInstance()->GetDeltaTime());
}

void Player::SetControlMode(ControlMode mode)
{
    if (controlMode_ != mode) {
        // 操作方式を変えた直後に、前のマウス操舵が残らないようにする。
        ResetSteering();
    }
    controlMode_ = mode;
}
// 加速・減速の入力を確認し、目標速度へ滑らかに近づける。
void Player::UpdateBoost(Input* input, bool isDebugMode)
{
    const PlayerInput::BoostInput boostInput = PlayerInput::ReadBoost(*input, !isDebugMode);
    if (movement_.UpdateBoost(boostInput.isBraking, boostInput.shouldBoost)) {
        SoundManager::GetInstance()->Play("BoostStart");
    }
}

void Player::EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight)
{
    movement_.EnableAllRangeMode(transform_, areaRadius, minHeight, maxHeight);
}

void Player::SetMouseSensitivity(float sensitivity)
{
    movement_.SetMouseSensitivity(sensitivity);
}

// 照準の中心からのずれにデッドゾーンと感度を反映し、操舵入力を滑らかにする。
void Player::UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad)
{
    movement_.UpdateSteering(aimPosition, screenSizePixels, isAllRangeMode, deltaTimeSeconds, isGamepad);
}

// レールの基準位置に左右・上下のずれを加え、ワールド位置へ変換する。
Vector3 Player::CalculateRailWorldPosition(const Vector3& railOffset) const
{
    return movement_.CalculateRailWorldPosition(railOffset);
}

// 機体の大きさとカメラ投影を考慮し、画面からはみ出ないレール上のずれへ補正する。
Vector3 Player::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    return movement_.ClampRailOffsetToScreen(railOffset);
}

// 状態の切替と開始処理を一か所で行う。
void Player::ChangeState(State state)
{
    if (state_ == state) {
        return;
    }
    state_ = state;
    if (state_ == State::Dead) {
        deadState_.Enter(*this);
    }
}
