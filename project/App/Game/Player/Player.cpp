// プレイヤーの照準・操作・移動・HP・死亡演出を扱う。武器と弾の管理はPlayerWeaponControllerへ任せる。
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
#include <numbers>

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
    if (!IsAlive()) {
        // 死亡後は通常操作へ進まず、落下演出と残っている弾だけを更新する。
        UpdateDeath();
        return;
    }
    Input* input = Input::GetInstance();
    if (input == nullptr) {
        return;
    }
    UpdateInvincibility(TimeManager::GetInstance()->GetDeltaTime());
    UpdateBoostState();
    if (!isDebugMode_) {
        // 照準を先に確定してから操舵し、発射と表示で参照する位置を揃える。
        // 使用中の入力機器から照準を取得し、操舵とレティクル表示に使う。
        UpdateAimFromInput(*input, TimeManager::GetInstance()->GetDeltaTime());
        ClampAimScreenPosition();
        ConstrainGamepadAimToPlayer();
        UpdateMovement(input);
    }
    ApplyRailPosition();
    if (!isDebugMode_) {
        ConstrainGamepadAimToPlayer();
        if (camera_ != nullptr) {
            // 移動後の機体との距離を補正した照準で、発射に使う奥行きを確定する。
            UpdateSmoothedAimDistance(*camera_, TimeManager::GetInstance()->GetDeltaTime());
        }
    }
    ApplyTransform();
    // 最新の照準と移動後の銃口から発射し、弾はこの後で1回だけ更新する。
    weaponController_.UpdateInput(input, isDebugMode_);
    weaponController_.UpdateBullets();
    // 寿命切れや命中で無効になった弾を一覧から取り除く。
    weaponController_.RemoveDeadBullets();
    object_->Update();
#ifdef _DEBUG
    weaponController_.DrawAimDebugLines();
#endif
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

// 死亡時の落下を進め、飛行中に使っていた操作状態を終了する。
void Player::UpdateDeath()
{
    // 落下中だけ機体姿勢を変える。落下終了後の爆発はシーン側が担当する。
    if (IsFalling()) {
        UpdateDeathAnimation(transform_, TimeManager::GetInstance()->GetDeltaTime());
        ApplyTransform();
    }
    weaponController_.UpdateBullets();
    weaponController_.RemoveDeadBullets();
    object_->Update();
}

void Player::SetCamera(Camera* camera)
{
    camera_ = camera;
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
        StopMovement();
        weaponController_.ResetHomingLock();
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

// 照準を画面中央に置き、狙う距離を基準値に戻す。
void Player::InitializeAim()
{

    aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) / 2.0f;
    aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) / 2.0f;
    smoothedAimDistance_ = kAimConvergenceDistance;
    gamepadAimOffset_ = {};
    hasGamepadAimAnchor_ = false;
}
// 照準画像がはみ出さないよう、画面端から半分のサイズだけ内側に制限する。
void Player::ClampAimScreenPosition()
{
    constexpr float kHalfAimSizePixels = 64.0f;

    if (aimScreenPosition_.x < kHalfAimSizePixels) {
        aimScreenPosition_.x = kHalfAimSizePixels;
    }

    if (aimScreenPosition_.x > static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - kHalfAimSizePixels) {
        aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - kHalfAimSizePixels;
    }

    if (aimScreenPosition_.y < kHalfAimSizePixels) {
        aimScreenPosition_.y = kHalfAimSizePixels;
    }

    if (aimScreenPosition_.y > static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - kHalfAimSizePixels) {
        aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - kHalfAimSizePixels;
    }
}

// 画面上の機体から照準が離れすぎないように制限し、入力の内部位置も揃える。
void Player::ClampGamepadAimToPlayer(const Vector2& playerScreenPosition)
{
    hasGamepadAimAnchor_ = false;
    const float width = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float height = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    constexpr float kHalfAimSizePixels = 64.0f;
    if (width <= kHalfAimSizePixels * 2.0f || height <= kHalfAimSizePixels * 2.0f ||
        !std::isfinite(playerScreenPosition.x) || !std::isfinite(playerScreenPosition.y)) {
        return;
    }
    // 画面外への投影時にも、照準画像が収まる領域を基準にする。
    const Vector2 anchor = {
        std::clamp(playerScreenPosition.x, kHalfAimSizePixels, width - kHalfAimSizePixels),
        std::clamp(playerScreenPosition.y, kHalfAimSizePixels, height - kHalfAimSizePixels)
    };
    gamepadAimAnchor_ = anchor;
    hasGamepadAimAnchor_ = true;
    ClampAimScreenPosition();
    const float dx = aimScreenPosition_.x - anchor.x;
    const float dy = aimScreenPosition_.y - anchor.y;
    const float maxDistance = (std::min)(width, height) * kGamepadMaxAimDistanceRatio;
    const float distanceSquared = dx * dx + dy * dy;
    if (distanceSquared > maxDistance * maxDistance) {
        const float scale = maxDistance / std::sqrt(distanceSquared);
        aimScreenPosition_ = { anchor.x + dx * scale, anchor.y + dy * scale };
    }
    // 表示位置と入力の内部位置を揃え、上限で倒し続けても余分な入力を蓄積しない。
    gamepadAimOffset_ = {
        (aimScreenPosition_.x - width * 0.5f) / (width * 0.5f),
        (aimScreenPosition_.y - height * 0.5f) / (height * 0.5f)
    };
}

// 機体からの照準のずれを、操舵処理が使う画面座標へ変換する。
Vector2 Player::GetGamepadSteeringScreenPosition() const
{
    if (!hasGamepadAimAnchor_) {
        return aimScreenPosition_;
    }
    const float width = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float height = static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    const float maxDistance = (std::min)(width, height) * kGamepadMaxAimDistanceRatio;
    if (maxDistance <= 0.0f) {
        return aimScreenPosition_;
    }
    return {
        width * 0.5f + (aimScreenPosition_.x - gamepadAimAnchor_.x) / maxDistance * width * 0.5f,
        height * 0.5f + (aimScreenPosition_.y - gamepadAimAnchor_.y) / maxDistance * height * 0.5f
    };
}

// デスクトップ上のマウス位置を、ゲームの描画領域の左上が原点の座標に変換する。
void Player::UpdateMouseAim()
{

    HWND hwnd = WinApp::GetInstance()->GetHwnd();

    POINT mousePosition;
    GetCursorPos(&mousePosition);
    ScreenToClient(hwnd, &mousePosition);

    aimScreenPosition_.x = static_cast<float>(mousePosition.x);
    aimScreenPosition_.y = static_cast<float>(mousePosition.y);
    gamepadAimOffset_ = {};
    hasGamepadAimAnchor_ = false;
}

// スティック入力を照準の移動量に変換し、入力を離したら少しずつ中央へ戻す。
void Player::UpdateGamepadAim(float stickX, float stickY, bool isPitchInverted, float deltaTimeSeconds, float sensitivity)
{
    hasGamepadAimAnchor_ = false;
    const float halfWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) * 0.5f;
    const float halfHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) * 0.5f;
    // 中央付近はゆっくり、倒し込むほど速く動かす。
    const float magnitude = std::sqrt(stickX * stickX + stickY * stickY);
    const float response = 0.5f + 0.5f * (std::min)(magnitude, 1.0f);
    stickX *= response;
    stickY *= response;
    const float dt = (std::max)(deltaTimeSeconds, 0.0f);
    constexpr float kAimMoveSpeed = 1.8f;
    constexpr float kAimReturnSpeed = 0.8f;
    const float moveSpeed = kAimMoveSpeed * std::clamp(sensitivity, 0.5f, 2.0f);
    float pitchDirection = -1.0f;
    if (isPitchInverted) { pitchDirection = 1.0f; }
    if (magnitude > 0.0f) {
        // 操作中は速度として加算する。端までの距離で減速させない。
        gamepadAimOffset_.x += stickX * moveSpeed * dt;
        gamepadAimOffset_.y += stickY * pitchDirection * moveSpeed * dt;
    } else {
        // Inputのデッドゾーン内（スティックを離したとき）だけ中央へ戻す。
        const float remaining = std::exp(-kAimReturnSpeed * dt);
        gamepadAimOffset_.x *= remaining;
        gamepadAimOffset_.y *= remaining;
    }
    constexpr float kHalfAimSizePixels = 64.0f;
    const float limitX = (std::max)(halfWidth - kHalfAimSizePixels, 0.0f) / (std::max)(halfWidth, 1.0f);
    const float limitY = (std::max)(halfHeight - kHalfAimSizePixels, 0.0f) / (std::max)(halfHeight, 1.0f);
    // 照準画像が収まる位置まで動かせるよう、円形ではなく画面の矩形で制限する。
    gamepadAimOffset_.x = std::clamp(gamepadAimOffset_.x, -limitX, limitX);
    gamepadAimOffset_.y = std::clamp(gamepadAimOffset_.y, -limitY, limitY);
    aimScreenPosition_.x = halfWidth + gamepadAimOffset_.x * halfWidth;
    aimScreenPosition_.y = halfHeight + gamepadAimOffset_.y * halfHeight;
}

// 画面上の照準を通って奥へ伸びるレイを作り、引数のaimRayに結果を書き込む。
void Player::CreateAimRay(Ray& aimRay, const Camera& activeCamera) const
{
    float mouseX = aimScreenPosition_.x;
    float mouseY = aimScreenPosition_.y;

    float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());

    // ピクセル座標をNDCへ変換する。画面中央は(0, 0)、左上は(-1, 1)。
    // 画面座標は下向きが正なので、Yを反転する。
    float ndcX = (2.0f * mouseX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / screenHeight);

    // 描画時の座標変換を逆にたどるため、投影行列とビュー行列の逆行列を用意する。
    Matrix4x4 inverseProjection = MatrixMath::Inverse(activeCamera.GetProjectionMatrix());
    Matrix4x4 inverseView = MatrixMath::Inverse(activeCamera.GetViewMatrix());

    // 同じ照準位置のニア面とファー面に点を置く。Zの0と1はNDCの奥行き。
    Vector3 nearPoint = { ndcX, ndcY, 0.0f };
    Vector3 farPoint = { ndcX, ndcY, 1.0f };

    // 逆投影で、NDCからカメラ基準の3D座標へ戻す。
    nearPoint = MatrixMath::Transform(nearPoint, inverseProjection);
    farPoint = MatrixMath::Transform(farPoint, inverseProjection);

    // 逆ビュー変換で、カメラ基準の座標からワールド座標へ戻す。
    nearPoint = MatrixMath::Transform(nearPoint, inverseView);
    farPoint = MatrixMath::Transform(farPoint, inverseView);

    // ニア面の点を始点にする。方向は手前から奥へ向け、長さを1に揃える。
    aimRay.origin = nearPoint;
    aimRay.direction = Normalize(farPoint - nearPoint);
}

// 対象までの距離に関係なく、レイ上の基準距離にある点を返す。
Vector3 Player::CreateConvergencePoint(const Ray& aimRay) const
{
    return aimRay.origin + aimRay.direction * kAimConvergenceDistance;
}

// 照準方向で最も近くに当たる対象までの距離へ、現在の照準距離を近づける。
void Player::UpdateSmoothedAimDistance(const Camera& activeCamera, float deltaTimeSeconds)
{
    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);

    // 何も当たらなければ基準距離を使い、当たれば命中点までの距離を使う。
    float targetDistance = kAimConvergenceDistance;
    RaycastHit hit {};
    if (CollisionManager::GetInstance()->Raycast(aimRay, hit)) {
        targetDistance = hit.distance;
    }

    // 対象が切り替わっても弾の収束位置が急変しないよう、奥行きだけを補間する。
    // 経過秒数から追従率を求め、フレームレートによる追従速度の差を抑える。
    const float followRate = 1.0f - std::exp(-kAimDistanceFollowSpeed * deltaTimeSeconds);
    smoothedAimDistance_ += (targetDistance - smoothedAimDistance_) * followRate;
}

// 補間済みの距離を使い、銃口から弾を向けるためのワールド座標を返す。
Vector3 Player::ResolveAimPoint(const Ray& aimRay) const
{
    return aimRay.origin + aimRay.direction * smoothedAimDistance_;
}

// 使用中のパッドを優先し、パッドでなければ設定された操作方式を選ぶ。
Player::ControlInputType Player::GetControlInputType(const Input& input) const
{
    if (input.IsUsingGamepad()) { return ControlInputType::Gamepad; }
    if (controlMode_ == ControlMode::StarFox) { return ControlInputType::Mouse; }
    return ControlInputType::Keyboard;
}

// 選択された操作方式に合わせて、マウスまたはパッドの照準を更新する。
void Player::UpdateAimFromInput(const Input& input, float deltaTimeSeconds)
{
    if (GetControlInputType(input) == ControlInputType::Gamepad) {
        UpdateGamepadAim(input.GetGamepadLeftStickX(), input.GetGamepadLeftStickY(),
            input.IsGamepadPitchInverted(), deltaTimeSeconds, input.GetGamepadAimSensitivity());
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
    return { steeringInput_, 2.75f };
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
    Vector2 steering {};
    if (input.IsKeyPressed(DIK_A)) { steering.x -= 1.0f; }
    if (input.IsKeyPressed(DIK_D)) { steering.x += 1.0f; }
    if (input.IsKeyPressed(DIK_W)) { steering.y += 1.0f; }
    if (input.IsKeyPressed(DIK_S)) { steering.y -= 1.0f; }
    return { steering, 1.0f };
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

    const bool isVisible = invincibleRemainingSeconds_ <= 0.000001f || static_cast<int>(invincibleRemainingSeconds_ * 15.0f + 0.00001f) % 2 == 0;
    return IsFalling() || (IsAlive() && isVisible);
}

// ダメージを受ける。無敵時間を確認し、HPを減らす。死亡時は落下演出へ進める。
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
    if (currentHp_ == 0) {
        deathState_ = DeathState::Falling;
        deathTimerSeconds_ = 0.0f;
        deathFallVelocity_ = 0.0f;
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
// 死亡時の落下・回転を更新する。落下演出が終わると死亡爆発へ進める。
void Player::UpdateDeathAnimation(EulerTransform& transform, float deltaTimeSeconds)
{
    if (deathState_ != DeathState::Falling) {
        return;
    }

    deathTimerSeconds_ += deltaTimeSeconds;
    deathFallVelocity_ += 18.0f * deltaTimeSeconds;
    transform.translate.y -= deathFallVelocity_ * deltaTimeSeconds;
    transform.rotate.x += 1.4f * deltaTimeSeconds;
    transform.rotate.z += 3.2f * deltaTimeSeconds;

    if (deathTimerSeconds_ >= kDeathFallDurationSeconds) {
        deathState_ = DeathState::Exploded;
    }
}

// 初期位置と姿勢を移動用の内部状態へ揃える。
void Player::InitializeMovement()
{
    railBasePosition_ = transform_.translate;
    railOffset_ = {};
}

// 外部から設定された機体位置を、レール基準の移動状態へ反映する。
void Player::SynchronizePosition()
{
    // 現在のオフセットを維持し、次のレール位置計算でも設定位置を再現する。
    railBasePosition_ = transform_.translate - railRight_ * railOffset_.x -
        railUp_ * railOffset_.y - railForward_ * railOffset_.z;
}

// 外部から設定された回転角を、全方向飛行の機首方向へ反映する。
void Player::SynchronizeOrientation()
{
    if (!isAllRangeMode_) {
        return;
    }
    flightYaw_ = -transform_.rotate.y;
    flightPitch_ = -transform_.rotate.x;
    railForward_ = { std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_), std::cos(flightYaw_) * std::cos(flightPitch_) };
    railRight_ = Normalize(Cross(Vector3{ 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
}

// 操作入力を読み、レール飛行か全方向飛行かに応じて移動処理を選ぶ。
void Player::UpdateMovement(Input* input)
{
    manualBank_ = 0.0f;
    if (input->IsGamepadButtonPressed(XINPUT_GAMEPAD_LEFT_SHOULDER)) { manualBank_ += 0.9f; }
    if (input->IsGamepadButtonPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER)) { manualBank_ -= 0.9f; }
    if (isAllRangeMode_) {
        UpdateAllRangeMove(input);
    } else {
        const ControlInput control = GetControlInput(*input,
            { static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
              static_cast<float>(WinApp::GetInstance()->GetClientHeight()) },
            false, TimeManager::GetInstance()->GetDeltaTime());
        Vector3 nextRailOffset = railOffset_;
        nextRailOffset.x += control.steering.x * moveSpeed_ * control.railResponse;
        nextRailOffset.y += control.steering.y * moveSpeed_ * control.railResponse;
        railOffset_ = ClampRailOffsetToScreen(nextRailOffset);
    }
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    UpdateRoll(transform_,
        input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_LEFT_SHOULDER),
        input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_RIGHT_SHOULDER), deltaTimeSeconds);
}

// レール基準位置と操縦によるずれを合成し、画面内に収まる機体位置を決める。
void Player::ApplyRailPosition()
{
    if (!isAllRangeMode_) {
        transform_.translate = CalculateRailWorldPosition(railOffset_);
    }
}

// 死亡や操作停止時に、速度とロールなどの移動状態を解除する。
void Player::StopMovement()
{
    // 死亡時に操作状態を解除する。速度や位置を0にする処理ではない。
    isBoosting_ = false;
    StopRoll();
    manualBank_ = 0.0f;
}

Vector3 Player::GetAutomaticMovementVelocity() const
{
    if (isAllRangeMode_) {
        return railForward_ * velocity_.z;
    }
    return railForward_ * normalMaxSpeed_;
}

void Player::SetRailFrame(const Vector3& railBasePosition,
    const Vector3& railRight, const Vector3& railUp, const Vector3& railForward)
{
    railBasePosition_ = railBasePosition;
    railRight_ = railRight;
    railUp_ = railUp;
    railForward_ = railForward;
}

void Player::ApplyRailAreaForce(const Vector3& force)
{
    railOffset_.x += force.x;
    railOffset_.y += force.y;
    railOffset_ = ClampRailOffsetToScreen(railOffset_);
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
    constexpr float kBrakeSpeedMultiplier = 0.5f;
    const bool isBraking = !isDebugMode &&
        (input->IsKeyPressed(DIK_LCONTROL) || input->IsKeyPressed(DIK_RCONTROL) ||
         input->IsGamepadButtonPressed(XINPUT_GAMEPAD_Y));
    const bool wasBoosting = isBoosting_;
    isBoosting_ = !isDebugMode && !isBraking &&
        (input->IsKeyPressed(DIK_LSHIFT) || input->IsMousePressed(1) ||
         input->IsGamepadButtonPressed(XINPUT_GAMEPAD_X));
    if (isBoosting_ && !wasBoosting) {
        // 押し続けている間は繰り返さず、ブースト開始時だけ効果音を鳴らす。
        SoundManager::GetInstance()->Play("BoostStart");
    }
    // Ctrlの減速を優先し、左右・上下の操作速度は通常時のまま保つ。
    velocity_.z = normalMaxSpeed_;
    moveSpeed_ = normalAcceleration_;
    if (isBraking) {
        velocity_.z = normalMaxSpeed_ * kBrakeSpeedMultiplier;
    } else if (isBoosting_) {
        velocity_.z = boostMaxSpeed_;
        moveSpeed_ = boostAcceleration_;
    }
}

void Player::EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight)
{
    isAllRangeMode_ = true;
    flightAreaRadius_ = areaRadius;
    flightMinHeight_ = minHeight;
    flightMaxHeight_ = maxHeight;
    // 切り替え前の機体姿勢を引き継ぎ、飛行方向と直交する左右・上下の軸を作る。
    flightYaw_ = -transform_.rotate.y;
    flightPitch_ = -transform_.rotate.x;
    railForward_ = {
        std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_),
        std::cos(flightYaw_) * std::cos(flightPitch_)
    };
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
    isReturningToFlightArea_ = false;
    railOffset_ = {};
    ResetSteering();
}

// 機首方向・飛行範囲への復帰・高度・位置と傾きを順に更新する。
void Player::UpdateAllRangeMove(Input* input)
{
    Vector2 steering = GetControlInput(*input,
        { static_cast<float>(WinApp::GetInstance()->GetClientWidth()),
          static_cast<float>(WinApp::GetInstance()->GetClientHeight()) },
        true, TimeManager::GetInstance()->GetDeltaTime()).steering;
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    // 手動旋回の後に範囲復帰と高度補正を加え、その姿勢で移動・傾きを更新する。
    UpdateFlightOrientation(steering, deltaTimeSeconds);
    UpdateFlightAreaReturn(steering, deltaTimeSeconds);
    UpdateFlightHeight(deltaTimeSeconds);
    UpdateFlightPosition(deltaTimeSeconds);
    if (!IsRolling()) {
        UpdateFlightBank(steering, deltaTimeSeconds);
    }
}

// 操舵入力で機首の上下角と左右角を変化させ、進行方向を求める。
void Player::UpdateFlightOrientation(const Vector2& steering, float deltaTimeSeconds)
{
    constexpr float kYawSpeed = 1.35f;
    constexpr float kPitchSpeed = 0.85f;
    constexpr float kMaxPitch = 0.95f;
    flightYaw_ += steering.x * kYawSpeed * deltaTimeSeconds;
    flightPitch_ = std::clamp(
        flightPitch_ + steering.y * kPitchSpeed * deltaTimeSeconds, -kMaxPitch, kMaxPitch);
}

// 飛行可能な範囲を超えた場合、範囲内へ戻るための操舵を加える。
void Player::UpdateFlightAreaReturn(Vector2& steering, float deltaTimeSeconds)
{
    constexpr float kYawSpeed = 1.35f;
    // 追従カメラも範囲内に収まるよう、機体が端に達する前に内側への旋回を始める。
    const float horizontalDistance = std::sqrt(
        transform_.translate.x * transform_.translate.x +
        transform_.translate.z * transform_.translate.z);
    // 復帰開始と終了の距離に差を設け、境界付近で状態が頻繁に切り替わるのを防ぐ。
    if (horizontalDistance > flightAreaRadius_ - 90.0f) {
        isReturningToFlightArea_ = true;
    } else if (horizontalDistance < flightAreaRadius_ - 150.0f) {
        isReturningToFlightArea_ = false;
    }
    if (isReturningToFlightArea_) {
        // 先に加えた手動の左右旋回を取り消し、原点方向へ自動旋回させる。
        flightYaw_ -= steering.x * kYawSpeed * deltaTimeSeconds;
        const float inwardYaw = std::atan2(-transform_.translate.x, -transform_.translate.z);
        const float yawDifference = std::remainder(
            inwardYaw - flightYaw_, 2.0f * std::numbers::pi_v<float>);
        flightYaw_ += std::clamp(yawDifference, -2.0f * deltaTimeSeconds, 2.0f * deltaTimeSeconds);
        steering.x = std::clamp(yawDifference, -1.0f, 1.0f);
    }
    flightYaw_ = std::remainder(flightYaw_, 2.0f * std::numbers::pi_v<float>);
}

// 上下の飛行限界を考慮して高度を補正する。
void Player::UpdateFlightHeight(float deltaTimeSeconds)
{
    // 高度の端へ向かう場合だけ、反対向きの緩やかなピッチへ近づける。
    if (transform_.translate.y < flightMinHeight_ + 12.0f && flightPitch_ < 0.0f) {
        flightPitch_ += (0.25f - flightPitch_) * (1.0f - std::exp(-4.0f * deltaTimeSeconds));
    }
    if (transform_.translate.y > flightMaxHeight_ - 12.0f && flightPitch_ > 0.0f) {
        flightPitch_ += (-0.25f - flightPitch_) * (1.0f - std::exp(-4.0f * deltaTimeSeconds));
    }
}

// 進行方向と速度から次の位置を求め、飛行範囲の制限を反映する。
void Player::UpdateFlightPosition(float deltaTimeSeconds)
{
    railForward_ = {
        std::sin(flightYaw_) * std::cos(flightPitch_),
        std::sin(flightPitch_),
        std::cos(flightYaw_) * std::cos(flightPitch_)
    };
    railRight_ = Normalize(Cross(Vector3 { 0.0f, 1.0f, 0.0f }, railForward_));
    railUp_ = Normalize(Cross(railForward_, railRight_));
    // 60FPS基準の前進量を経過秒数に換算し、フレームレートに応じて移動させる。
    transform_.translate += railForward_ * (velocity_.z * 60.0f * deltaTimeSeconds);
    const float distanceAfterMove = std::sqrt(
        transform_.translate.x * transform_.translate.x +
        transform_.translate.z * transform_.translate.z);
    if (distanceAfterMove > flightAreaRadius_) {
        // 自動復帰でも範囲外へ出た場合は、水平位置を円の境界へ戻す。
        const float correction = flightAreaRadius_ / distanceAfterMove;
        transform_.translate.x *= correction;
        transform_.translate.z *= correction;
    }
    transform_.translate.y = std::clamp(
        transform_.translate.y, flightMinHeight_, flightMaxHeight_);
    railBasePosition_ = transform_.translate;
    transform_.rotate.x = -flightPitch_;
    transform_.rotate.y = -flightYaw_;
}

// 旋回入力と手動バンクに合わせて機体を傾ける。
void Player::UpdateFlightBank(const Vector2& steering, float deltaTimeSeconds)
{
    const float targetBank = -steering.x * 0.65f + manualBank_;
    transform_.rotate.z += (targetBank - transform_.rotate.z) *
        (1.0f - std::exp(-7.0f * deltaTimeSeconds));
}

namespace {
// 使用箇所：本ファイルのUpdate。中央付近の小さな入力を無視し、意図しない操舵を防ぐ。
float ApplySteeringDeadZone(float value)
{
    constexpr float kDeadZone = 0.05f;
    const float magnitude = std::abs(value);
    if (magnitude <= kDeadZone) {
        return 0.0f;
    }
    // デッドゾーンの外側を0～1へ引き伸ばし、元の左右・上下の符号を戻す。
    return std::copysign((magnitude - kDeadZone) / (1.0f - kDeadZone), value);
}
}

void Player::SetMouseSensitivity(float sensitivity)
{
    // 極端に小さい・大きい感度にならないよう、設定値を制限する。
    mouseSensitivity_ = std::clamp(sensitivity, 0.5f, 2.0f);
}

// 照準の中心からのずれにデッドゾーンと感度を反映し、操舵入力を滑らかにする。
void Player::UpdateSteering(const Vector2& aimPosition, const Vector2& screenSizePixels,
    bool isAllRangeMode, float deltaTimeSeconds, bool isGamepad)
{
    const float screenWidth = screenSizePixels.x;
    const float screenHeight = screenSizePixels.y;
    if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
        // 画面サイズが無効なら、ゼロ除算を避けて現在の操舵入力を維持する。
        return;
    }

    // 照準をアナログスティックとして扱い、画面中央からのずれを入力へ変換する。
    // 中央は0、右端・上端は+1、左端・下端は-1。Yは画面座標と向きが逆になる。
    float inputX = (aimPosition.x - screenWidth * 0.5f) /(screenWidth * 0.5f);
    float inputY = (screenHeight * 0.5f - aimPosition.y) /(screenHeight * 0.5f);

    // 感度を掛けて-1～1へ制限し、中央付近の微小なずれを取り除く。
    // 照準の移動速度と機体の追従を分離する。パッドは照準のずれをそのまま操舵に使う。
    float sensitivity = mouseSensitivity_;
    if (isGamepad) { sensitivity = 1.0f; }
    inputX = std::clamp(inputX * sensitivity, -1.0f, 1.0f);
    inputY = std::clamp(inputY * sensitivity, -1.0f, 1.0f);
    // パッドのデッドゾーンはInputで適用済み。低感度でも細かな操作を残す。
    if (!isGamepad) {
        inputX = ApplySteeringDeadZone(inputX);
        inputY = ApplySteeringDeadZone(inputY);
    }

    // パッドは入力へ即時追従し、照準の移動・中央復帰にさらに遅れを重ねない。
    if (isGamepad) {
        steeringInput_ = { inputX, inputY };
        return;
    }
    // マウスのレール移動は即時追従し、全方向飛行では経過秒数から追従率を求める。
    // 全方向飛行の追従率は、フレームレートによる旋回感の差を抑えるため時間ベースにする。
    float steeringLerpRate = 1.0f;
    if (isAllRangeMode) {steeringLerpRate = 1.0f - std::exp(-13.4f * deltaTimeSeconds);
    }
    // 全方向飛行のマウス操舵には補間を適用する。
    steeringInput_.x +=(inputX - steeringInput_.x) * steeringLerpRate;
    steeringInput_.y +=(inputY - steeringInput_.y) * steeringLerpRate;

}

// 入力の取得は呼び出し元に任せ、渡された左右の押下でロールを制御する。
void Player::UpdateRoll(EulerTransform& transform, bool isLeftTriggered, bool isRightTriggered, float deltaTimeSeconds)
{
    if (isRolling_) {
        // ロール中は新しい入力を判定せず、0.5秒かけてZ軸を1回転させる。
        rollTimerSeconds_ += deltaTimeSeconds;
        float progress = static_cast<float>(rollTimerSeconds_) / static_cast<float>(kRollDurationSeconds);
        transform.rotate.z = rollDirection_ * progress * 2.0f * std::numbers::pi_v<float>;

        if (rollTimerSeconds_ + 0.000001f >= kRollDurationSeconds) {
            // 1回転したら傾きを0に戻し、次のロールまで1秒待つ。
            isRolling_ = false;
            transform.rotate.z = 0.0f;
            rollCooldownSeconds_ = kRollCooldownDurationSeconds;
        }
        return;
    }

    // ロールしていない間に、再使用までの待ち時間を減らす。
    if (rollCooldownSeconds_ > 0) {
        rollCooldownSeconds_ = (std::max)(0.0f, rollCooldownSeconds_ - deltaTimeSeconds);
    }

    // 最初の押下後の受付時間を減らす。0になったら次の押下は1回目として扱う。
    if (leftKeyTapTimerSeconds_ > 0.000001f) {
        leftKeyTapTimerSeconds_ = (std::max)(0.0f, leftKeyTapTimerSeconds_ - deltaTimeSeconds);
    }
    if (rightKeyTapTimerSeconds_ > 0.000001f) {
        rightKeyTapTimerSeconds_ = (std::max)(0.0f, rightKeyTapTimerSeconds_ - deltaTimeSeconds);
    }

    // 再使用可能なときだけ、同じ方向の2回目の押下でロールを開始する。
    if (rollCooldownSeconds_ <= 0.000001f) {
        if (isLeftTriggered) {
            if (leftKeyTapTimerSeconds_ > 0.000001f) {
                isRolling_ = true;
                rollTimerSeconds_ = 0;
                rollDirection_ = 1.0f;
                leftKeyTapTimerSeconds_ = 0;
                return;
            } else {
                // 左の1回目の押下。2回目を受け付けるタイマーを開始する。
                leftKeyTapTimerSeconds_ = kMaxTapIntervalSeconds;
            }
        }

        if (isRightTriggered) {
            if (rightKeyTapTimerSeconds_ > 0.000001f) {
                isRolling_ = true;
                rollTimerSeconds_ = 0;
                rollDirection_ = -1.0f;
                rightKeyTapTimerSeconds_ = 0;
                return;
            } else {
                // 右の1回目の押下。左右の受付時間は別々に管理する。
                rightKeyTapTimerSeconds_ = kMaxTapIntervalSeconds;
            }
        }
    }
}

// 機体の表示範囲が画面端を越えた量を求める。
Vector2 Player::CalculateScreenCorrection(const Vector3& railOffset) const
{
    Vector2 correction {};
    correction.x = 0.0f;
    correction.y = 0.0f;

    if (camera_ == nullptr) {
        return correction;
    }

    Vector3 playerPosition = CalculateRailWorldPosition(railOffset);
    Vector2 screenPosition = camera_->WorldToScreen(playerPosition);

    float minX = screenPosition.x;
    float maxX = screenPosition.x;
    float minY = screenPosition.y;
    float maxY = screenPosition.y;

    // 機体の中心だけでなく、レールの左右・上下に広がる矩形の4隅も投影する。
    Vector3 rightExtent = railRight_ * playerBoundsHalfWidth_;
    Vector3 upExtent = railUp_ * playerBoundsHalfHeight_;

    UpdateScreenBounds(playerPosition + rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition + rightExtent - upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent - upExtent, minX, maxX, minY, maxY);

    // 画面端の余白を除いた範囲を、機体を収める領域とする。
    float leftLimit = playerClampMarginX_;
    float rightLimit = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - playerClampMarginX_;

    float topLimit = playerClampMarginY_;
    float bottomLimit = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - playerClampMarginY_;

    // はみ出した側を画面内へ戻す量を求める。まだワールド座標の移動量には変換しない。
    if (minX < leftLimit) {
        correction.x = leftLimit - minX;
    } else if (maxX > rightLimit) {
        correction.x = rightLimit - maxX;
    }

    if (minY < topLimit) {
        correction.y = topLimit - minY;
    } else if (maxY > bottomLimit) {
        correction.y = bottomLimit - maxY;
    }

    return correction;
}

// レールの基準位置に左右・上下のずれを加え、ワールド位置へ変換する。
Vector3 Player::CalculateRailWorldPosition(const Vector3& railOffset) const
{
    // 基準位置に、レールの左右・上下・前後方向の移動量を足す。
    return railBasePosition_ + railRight_ * railOffset.x + railUp_ * railOffset.y + railForward_ * railOffset.z;
}

// 機体の大きさとカメラ投影を考慮し、画面からはみ出ないレール上のずれへ補正する。
Vector3 Player::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    Vector3 correctedRailOffset = railOffset;
    // 先にレール上の移動範囲へ制限する。前後方向のオフセットは使わない。
    correctedRailOffset.x = std::clamp(
        correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
    correctedRailOffset.y = std::clamp(
        correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
    correctedRailOffset.z = 0.0f;

    if (camera_ == nullptr) {
        return correctedRailOffset;
    }

    // 投影による誤差を減らすため、補正を最大3回繰り返す。
    // カメラが未設定なら上の移動範囲の制限だけで終了する。
    constexpr int kCorrectionCount = 3;
    for (int correctionIndex = 0; correctionIndex < kCorrectionCount; ++correctionIndex) {
        Vector2 screenCorrection = CalculateScreenCorrection(correctedRailOffset);

        if (std::fabs(screenCorrection.x) < 0.01f && std::fabs(screenCorrection.y) < 0.01f) {
            break;
        }

        // 左右・上下へそれぞれ1単位動かした場合の、画面上の変化量を調べる。
        Vector2 baseScreen = camera_->WorldToScreen(CalculateRailWorldPosition(correctedRailOffset));

        Vector3 rightOffset = correctedRailOffset;
        rightOffset.x += 1.0f;
        Vector2 rightScreen = camera_->WorldToScreen(CalculateRailWorldPosition(rightOffset));

        Vector3 upOffset = correctedRailOffset;
        upOffset.y += 1.0f;
        Vector2 upScreen = camera_->WorldToScreen(CalculateRailWorldPosition(upOffset));

        float rightScreenX = rightScreen.x - baseScreen.x;
        float rightScreenY = rightScreen.y - baseScreen.y;
        float upScreenX = upScreen.x - baseScreen.x;
        float upScreenY = upScreen.y - baseScreen.y;

        // 画面上の補正量を、レールの左右・上下の移動量へ変換するための行列式。
        float determinant = rightScreenX * upScreenY - rightScreenY * upScreenX;

        if (std::fabs(determinant) > 0.0001f) {
            float offsetX = (screenCorrection.x * upScreenY - screenCorrection.y * upScreenX) / determinant;
            float offsetY = (rightScreenX * screenCorrection.y - rightScreenY * screenCorrection.x) / determinant;

            correctedRailOffset.x += offsetX;
            correctedRailOffset.y += offsetY;
        } else {
            // 投影した2方向がほぼ重なる場合は、各方向への成分を使って補正する。
            float rightLengthSquared = rightScreenX * rightScreenX + rightScreenY * rightScreenY;
            if (rightLengthSquared > 0.0001f) {
                float offsetX = (screenCorrection.x * rightScreenX + screenCorrection.y * rightScreenY) / rightLengthSquared;
                correctedRailOffset.x += offsetX;
            }

            float upLengthSquared = upScreenX * upScreenX + upScreenY * upScreenY;
            if (upLengthSquared > 0.0001f) {
                float offsetY = (screenCorrection.x * upScreenX + screenCorrection.y * upScreenY) / upLengthSquared;
                correctedRailOffset.y += offsetY;
            }
        }

        // 画面内補正でレールの移動範囲を超えないよう、もう一度制限する。
        correctedRailOffset.x = std::clamp(correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
        correctedRailOffset.y = std::clamp(correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
        correctedRailOffset.z = 0.0f;
    }

    return correctedRailOffset;
}

// 機体周辺の点を画面へ投影し、表示範囲の最小・最大座標を更新する。
void Player::UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const
{
    if (camera_ == nullptr) {
        return;
    }

    // ワールド座標を画面へ投影し、左右・上下の最小値と最大値を更新する。
    Vector2 screenPosition = camera_->WorldToScreen(worldPosition);
    minX = (std::min)(minX, screenPosition.x);
    maxX = (std::max)(maxX, screenPosition.x);
    minY = (std::min)(minY, screenPosition.y);
    maxY = (std::max)(maxY, screenPosition.y);
}
