#include "Player.h"
#include "ChargeBeamBullet.h"
#include "MissileBullet.h"
#include "../../Engine/3D/ModelManager.h"
#include "../../Engine/3D/Object3dManager.h"
#include "../../Engine/Effect/EffectManager.h"
#include "../../Engine/Input/Input.h"
#include "../../Engine/debugcamera/DebugCameraController.h"
#include <algorithm>
#include <cassert>

#include "../../externals/imgui/ImGuizmo.h"

#include "../../Engine/EditorManager/EditorManager.h"
void Player::Initialize(Model* model)
{
    assert(model != nullptr);

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }
    ModelManager::GetInstance()->Load("star.obj"); //
    bulletModel_ = ModelManager::GetInstance()->Load("star.obj");

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, -2.0f, 0.0f };
    aimScreenPosition_.x = WinApp::GetInstance()->kClientWidth / 2.0f;
    aimScreenPosition_.y = WinApp::GetInstance()->kClientHeight / 2.0f;
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    aimScreenPosition_.x = WinApp::GetInstance()->kClientWidth / 2.0f;
    aimScreenPosition_.y = WinApp::GetInstance()->kClientHeight / 2.0f;
}

void Player::Update()
{
    if (object_ == nullptr) {
        return;
    }

    Input* input = Input::GetInstance();

    if (input == nullptr) {
        return;
    }

    if (invincibleTimer_ > 0) {
        --invincibleTimer_;
    }

    // デバッグカメラモードの取得
    if (debugCameraController_ != nullptr) {
        isDebugMode = debugCameraController_->GetDebugMode();
    }
    // plaerの移動速度
 
    isBoosting_ = input->IsKeyPressed(DIK_LSHIFT);
    velocity_.z = normalMaxSpeed_;
    moveSpeed_ = normalAcceleration_;
    if (isBoosting_) {
        velocity_.z = boostMaxSpeed_;
        moveSpeed_ = boostAcceleration_;
    }
    transform_.translate.z += velocity_.z;
    UpdateWeaponSwitch(input);
    if (missileFireCooldownFrames_ < kMissileFireIntervalFrames) {
        ++missileFireCooldownFrames_;
    }
    // デバッグカメラモードでないときは、マウスで照準を動かし、キーボードでプレイヤーを動かす
    if (!isDebugMode) {
        UpdateMouseAim();
        UpdateKeyboardMove(input);
        ClampAimScreenPosition();
        ClampPlayerScreenPosition();
    }
    if (currentWeapon_ == kWeaponChargeBeam) {
        UpdateChargeBeam(input);
    } else {
        if (isChargeBeamCharging_) {
            StopChargeBeam();
        }

        wasChargeBeamButtonPressed_ = input->IsMousePressed(0);

        if (input->IsMouseTrigger(0)) {
            FireBullet();
        }
    }

    // transform_.translate.z += 0.5f;
    //  transform反映
    ApplyTransform();
    // 弾更新と死んだ弾の削除
    UpdateBullets();
    // 死んだ弾の削除
    RemoveDeadBullets();

    object_->Update();
}
#pragma endregion

void Player::Draw()
{
    if (object_ == nullptr) {
        return;
    }
    for (std::unique_ptr<Bullet>& bullet : bullets_) {

        bullet->Draw();
    }
    if (invincibleTimer_ <= 0 || (invincibleTimer_ / 4) % 2 == 0) {
        object_->Draw();
    }
}

void Player::SetCamera(Camera* camera)
{
    camera_ = camera;

    if (object_ != nullptr) {
        object_->SetCamera(camera_);
    }
}

void Player::SetDebugCameraController(DebugCameraController* debugCameraController)
{
    debugCameraController_ = debugCameraController;
}

bool Player::ApplyDamage(int damage)
{
    if (damage <= 0 || invincibleTimer_ > 0 || currentHp_ <= 0) {
        return false;
    }

    currentHp_ -= damage;
    if (currentHp_ < 0) {
        currentHp_ = 0;
    }
    invincibleTimer_ = kInvincibleFrames;
    return true;
}

// 照準の画面上の位置を制限する関数
void Player::ClampAimScreenPosition()
{
    float halfAimSize = 64.0f;

    // 左の制限64で固定
    if (aimScreenPosition_.x < halfAimSize) {
        aimScreenPosition_.x = halfAimSize;
    }

    // 右の制限はウィンドウ幅-64
    if (aimScreenPosition_.x > WinApp::GetInstance()->kClientWidth - halfAimSize) {
        aimScreenPosition_.x = WinApp::GetInstance()->kClientWidth - halfAimSize;
    }

    // 上の制限64で固定
    if (aimScreenPosition_.y < halfAimSize) {
        aimScreenPosition_.y = halfAimSize;
    }

    // 下の制限はウィンドウ高さ-64
    if (aimScreenPosition_.y > WinApp::GetInstance()->kClientHeight - halfAimSize) {
        aimScreenPosition_.y = WinApp::GetInstance()->kClientHeight - halfAimSize;
    }
}

// マウスで照準を動かす関数
void Player::UpdateMouseAim()
{
    HWND hwnd = WinApp::GetInstance()->GetHwnd();

    POINT mousePosition;
    GetCursorPos(&mousePosition);
    ScreenToClient(hwnd, &mousePosition);

    aimScreenPosition_.x = static_cast<float>(mousePosition.x);
    aimScreenPosition_.y = static_cast<float>(mousePosition.y);
}

void Player::ClampPlayerScreenPosition()
{
    if (camera_ == nullptr) {
        return;
    }

    Vector2 screenPosition = camera_->WorldToScreen(transform_.translate);

    float leftLimit = 100.0f;
    float rightLimit = WinApp::GetInstance()->kClientWidth - 100.0f;

    float topLimit = 100.0f;
    float bottomLimit = WinApp::GetInstance()->kClientHeight - 100.0f;

    if (screenPosition.x < leftLimit) {
        transform_.translate.x += moveSpeed_;
    }

    if (screenPosition.x > rightLimit) {
        transform_.translate.x -= moveSpeed_;
    }

    if (screenPosition.y < topLimit) {
        transform_.translate.y -= moveSpeed_;
    }

    if (screenPosition.y > bottomLimit) {
        transform_.translate.y += moveSpeed_;
    }
}

// 弾を発射する関数
void Player::FireBullet()
{
    if (currentWeapon_ == kWeaponChargeBeam) {
        return;
    }

    if (bulletModel_ == nullptr) {
        return;
    }

    if (camera_ == nullptr) {
        return;
    }

    if (currentWeapon_ == kWeaponMissileBullet) {
        if (missileFireCooldownFrames_ < kMissileFireIntervalFrames) {
            return;
        }
        missileFireCooldownFrames_ = 0;
    }

    float shotSpeed = bulletSpeed_;
    std::unique_ptr<Bullet> bullet = CreateBullet(shotSpeed);

    bullet->Initialize(bulletModel_);
    bullet->SetCamera(camera_);

    Vector3 bulletPosition = transform_.translate;
    bulletPosition.y += bulletSpawnOffsetY_;
    bulletPosition.z += 0.0f;

    Vector3 muzzleEffectPosition = transform_.translate;
    muzzleEffectPosition.y += bulletSpawnOffsetY_;
    muzzleEffectPosition.z += 6.0f;
    EffectManager::GetInstance()->PlayEffect("ShotBullet", muzzleEffectPosition);

    bullet->SetTranslate(bulletPosition);

    float mouseX = aimScreenPosition_.x;
    float mouseY = aimScreenPosition_.y;

    float screenWidth = static_cast<float>(WinApp::GetInstance()->kClientWidth);
    float screenHeight = static_cast<float>(WinApp::GetInstance()->kClientHeight);

    float ndcX = (2.0f * mouseX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / screenHeight);

    Matrix4x4 inverseProjection = MatrixMath::Inverse(camera_->GetProjectionMatrix());
    Matrix4x4 inverseView = MatrixMath::Inverse(camera_->GetViewMatrix());

    Vector3 nearPoint = { ndcX, ndcY, 0.0f };
    Vector3 farPoint = { ndcX, ndcY, 1.0f };

    nearPoint = MatrixMath::Transform(nearPoint, inverseProjection);
    farPoint = MatrixMath::Transform(farPoint, inverseProjection);

    nearPoint = MatrixMath::Transform(nearPoint, inverseView);
    farPoint = MatrixMath::Transform(farPoint, inverseView);

    Vector3 bulletDirection = Normalize(farPoint - bulletPosition);

    Vector3 bulletVelocity;
    bulletVelocity.x = bulletDirection.x * shotSpeed + velocity_.x;
    bulletVelocity.y = bulletDirection.y * shotSpeed + velocity_.y;
    bulletVelocity.z = bulletDirection.z * shotSpeed + velocity_.z;

    bullet->SetVelocity(bulletVelocity);
    bullet->OnFired();

    bullets_.push_back(std::move(bullet));
}

void Player::UpdateChargeBeam(Input* input)
{
    bool isMousePressed = input->IsMousePressed(0);

    if (input->IsMouseTrigger(0)) {
        StartChargeBeam();
    }

    if (isChargeBeamCharging_ && isMousePressed) {
        ++chargeBeamChargeFrames_;

        Vector3 chargeEffectPosition = GetChargeEffectPosition();
        if (chargeLoopHandle_ != kInvalidEffectHandle) {
            EffectManager::GetInstance()->SetEffectPosition(chargeLoopHandle_, chargeEffectPosition);
        }

        if (chargeBeamChargeFrames_ >= kChargeBeamLevel3Frames && !chargeBeamReadyPlayed_) {
            EffectManager::GetInstance()->PlayEffect("ChargeReady", chargeEffectPosition);
            chargeBeamReadyPlayed_ = true;
        }
    }

    if (isChargeBeamCharging_ && !isMousePressed && wasChargeBeamButtonPressed_) {
        FireChargeBeam();
        StopChargeBeam();
    }

    wasChargeBeamButtonPressed_ = isMousePressed;
}

void Player::StartChargeBeam()
{
    isChargeBeamCharging_ = true;
    chargeBeamReadyPlayed_ = false;
    chargeBeamChargeFrames_ = 0;

    Vector3 chargeEffectPosition = GetChargeEffectPosition();
    EffectManager::GetInstance()->PlayEffect("ChargeStart", chargeEffectPosition);
    chargeLoopHandle_ = EffectManager::GetInstance()->PlayLoopEffect("ChargeLoop", chargeEffectPosition);
}

void Player::FireChargeBeam()
{
    if (bulletModel_ == nullptr) {
        return;
    }

    if (camera_ == nullptr) {
        return;
    }

    int chargeLevel = GetChargeBeamLevel();

    std::unique_ptr<ChargeBeamBullet> chargeBeam = std::make_unique<ChargeBeamBullet>();
    chargeBeam->Initialize(bulletModel_);
    chargeBeam->SetCamera(camera_);

    Vector3 beamPosition = transform_.translate;
    beamPosition.y += bulletSpawnOffsetY_;
    beamPosition.z += 4.0f;

    chargeBeam->Configure(chargeLevel, beamPosition);
    bullets_.push_back(std::move(chargeBeam));

    EffectManager::GetInstance()->PlayEffect("BeamFire", beamPosition);

    if (chargeLevel >= 2) {
        Vector3 beamMiddlePosition = beamPosition;
        beamMiddlePosition.z += 45.0f;
        EffectManager::GetInstance()->PlayEffect("BeamFire", beamMiddlePosition);
    }

    if (chargeLevel >= 3) {
        Vector3 beamFarPosition = beamPosition;
        beamFarPosition.z += 90.0f;
        EffectManager::GetInstance()->PlayEffect("BeamFire", beamFarPosition);
        EffectManager::GetInstance()->PlayEffect("ChargeReady", beamPosition);
    }
}

void Player::StopChargeBeam()
{
    if (chargeLoopHandle_ != kInvalidEffectHandle) {
        EffectManager::GetInstance()->StopEffect(chargeLoopHandle_);
        chargeLoopHandle_ = kInvalidEffectHandle;
    }

    isChargeBeamCharging_ = false;
    chargeBeamReadyPlayed_ = false;
    chargeBeamChargeFrames_ = 0;
}

int Player::GetChargeBeamLevel() const
{
    if (chargeBeamChargeFrames_ >= kChargeBeamLevel3Frames) {
        return 3;
    }

    if (chargeBeamChargeFrames_ >= kChargeBeamLevel2Frames) {
        return 2;
    }

    return 1;
}

float Player::GetChargeBeamRate() const
{
    float chargeRate =
        static_cast<float>(chargeBeamChargeFrames_) /
        static_cast<float>(kChargeBeamLevel3Frames);

    if (chargeRate > 1.0f) {
        chargeRate = 1.0f;
    }

    if (chargeRate < 0.0f) {
        chargeRate = 0.0f;
    }

    return chargeRate;
}

Vector3 Player::GetChargeEffectPosition() const
{
    Vector3 chargeEffectPosition = transform_.translate;
    chargeEffectPosition.y += bulletSpawnOffsetY_;
    chargeEffectPosition.z += 3.2f;
    return chargeEffectPosition;
}

std::unique_ptr<Bullet> Player::CreateBullet(float& shotSpeed)
{
    switch (currentWeapon_) {
    case kWeaponMissileBullet:
    {
        std::unique_ptr<MissileBullet> missile = std::make_unique<MissileBullet>();
        shotSpeed = missile->GetSpeed() / 60.0f;
        return missile;
    }
    case kWeaponChargeBeam:
        shotSpeed = 0.0f;
        return std::make_unique<Bullet>();
    case kWeaponNormalBullet:
    default:
        shotSpeed = bulletSpeed_;
        return std::make_unique<Bullet>();
    }
}

void Player::UpdateWeaponSwitch(Input* input)
{
    if (input->GetMouseWheel() > 0) {
        currentWeapon_ = (currentWeapon_ + 1) % kWeaponCount;
    }

    if (input->GetMouseWheel() < 0) {
        currentWeapon_ = (currentWeapon_ + kWeaponCount - 1) % kWeaponCount;
    }
}

const char* Player::GetCurrentWeaponName() const
{
    switch (currentWeapon_) {
    case kWeaponChargeBeam:
        return "ChargeBeam";
    case kWeaponMissileBullet:
        return "MissileBullet";
    case kWeaponNormalBullet:
    default:
        return "NormalBullet";
    }
}

// 弾更新
void Player::UpdateBullets()
{
    for (std::unique_ptr<Bullet>& bullet : bullets_) {
        bullet->Update();
    }
}

// 死んだ弾を削除する関数
void Player::RemoveDeadBullets()
{
    for (uint32_t i = 0; i < bullets_.size();) {
        if (!bullets_[i]->IsAlive()) {
            bullets_.erase(bullets_.begin() + i);
        } else {
            ++i;
        }
    }
}

// transform反映
void Player::ApplyTransform()
{
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void Player::UpdateKeyboardMove(Input* input)
{
    if (input->IsKeyPressed(DIK_A)) {
        transform_.translate.x -= moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_D)) {
        transform_.translate.x += moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_W)) {
        transform_.translate.y += moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_S)) {
        transform_.translate.y -= moveSpeed_;
    }
}
#ifdef _DEBUG

void Player::DrawImGui()
{
    ImGui::Begin("Player");

    ImGui::Text("Position");

    ImGui::Text("X : %.2f", transform_.translate.x);
    ImGui::Text("Y : %.2f", transform_.translate.y);
    ImGui::Text("Z : %.2f", transform_.translate.z);

    ImGui::Separator();

    ImGui::Text("Velocity");

    ImGui::Text("X : %.2f", velocity_.x);
    ImGui::Text("Y : %.2f", velocity_.y);
    ImGui::Text("Z : %.2f", velocity_.z);
    ImGui::Separator();

    ImGui::Text("Weapon No : %d", currentWeapon_);
    ImGui::Text("Weapon Name : %s", GetCurrentWeaponName());
    ImGui::Text("Charge Level : %d", GetChargeBeamLevel());
    ImGui::ProgressBar(GetChargeBeamRate(), ImVec2(160.0f, 0.0f), "Charge");
    ImGui::Separator();

    ImGui::Text("HP : %d / %d", currentHp_, maxHp_);
    ImGui::Text("Invincible : %d", invincibleTimer_);
    ImGui::Separator();

    ImGui::Text("Move Limit");

    ImGui::DragFloat(
        "MoveLimitX",
        &moveLimitX_,
        0.1f);

    ImGui::DragFloat(
        "MoveLimitY",
        &moveLimitY_,
        0.1f);

    ImGui::DragFloat(
        "ClampMarginX",
        &playerClampMarginX_,
        0.1f);

    ImGui::DragFloat(
        "ClampMarginY",
        &playerClampMarginY_,
        0.1f);
    ImGui::End();
}

#endif // _DEBUG
