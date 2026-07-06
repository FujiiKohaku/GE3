#include "App/Game/Player/Player.h"
#include "App/Game/Player/Bullet/MissileBullet.h"
#include "App/Game/Player/Bullet/NormalBullet.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Input/Input.h"
#include "Engine/debugcamera/DebugCameraController.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#ifdef _DEBUG
#include "externals/imgui/ImGuizmo.h"
#endif

#include "Engine/EditorManager/EditorManager.h"
#include "Engine/Debug/DebugRenderer.h"

namespace {
constexpr float kAimConvergenceDistance = 220.0f;
}

void Player::Initialize(Model* model)
{
    assert(model != nullptr);

    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dManager::GetInstance());
    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }
    ModelManager::GetInstance()->Load("Weapons/Star/star.obj");
    bulletModel_ = ModelManager::GetInstance()->Load("Weapons/Star/star.obj");

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, -2.0f, 0.0f };
    railBasePosition_ = transform_.translate;
    railOffset_ = { 0.0f, 0.0f, 0.0f };
    aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) / 2.0f;
    aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) / 2.0f;
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) / 2.0f;
    aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) / 2.0f;
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
    UpdateWeaponSwitch(input);
    if (missileFireCooldownFrames_ < kMissileFireIntervalFrames) {
        ++missileFireCooldownFrames_;
    }
    // デバッグカメラモードでないときは、マウスで照準を動かし、キーボードでプレイヤーを動かす
    if (!isDebugMode) {
        UpdateMouseAim();
        UpdateKeyboardMove(input);
        ClampAimScreenPosition();
    }

    transform_.translate = CalculateRailWorldPosition(railOffset_);

    //  transform反映
    ApplyTransform();
    // 弾更新と死んだ弾の削除
    UpdateBullets();
    // 死んだ弾の削除
    RemoveDeadBullets();

    object_->Update();

#ifdef _DEBUG
    if (drawDebugLines_) {
        // 1. AimCameraから飛ぶRay (青)
        DebugRenderer::GetInstance()->AddLine(debugAimRayOrigin_, debugAimPoint_, { 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f);

        // 2. Playerのマズルから飛ぶ実際の弾の進行方向 (赤)
        DebugRenderer::GetInstance()->AddLine(debugMuzzlePosition_, debugAimPoint_, { 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f);

        // 3. 描画用Cameraから飛ぶRay (黄色 - デバッグ比較専用)
        DebugRenderer::GetInstance()->AddLine(debugDrawRayOrigin_, debugDrawAimPoint_, { 1.0f, 1.0f, 0.0f, 1.0f }, 3.0f);
    }
#endif
}
#pragma endregion

void Player::Draw()
{
    if (object_ == nullptr) {
        return;
    }
    for (std::unique_ptr<PlayerBullet>& bullet : bullets_) {

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
    if (aimScreenPosition_.x > static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - halfAimSize) {
        aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - halfAimSize;
    }

    // 上の制限64で固定
    if (aimScreenPosition_.y < halfAimSize) {
        aimScreenPosition_.y = halfAimSize;
    }

    // 下の制限はウィンドウ高さ-64
    if (aimScreenPosition_.y > static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - halfAimSize) {
        aimScreenPosition_.y = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - halfAimSize;
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

    Vector3 rightExtent = railRight_ * playerBoundsHalfWidth_;
    Vector3 upExtent = railUp_ * playerBoundsHalfHeight_;

    UpdateScreenBounds(playerPosition + rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition + rightExtent - upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent + upExtent, minX, maxX, minY, maxY);
    UpdateScreenBounds(playerPosition - rightExtent - upExtent, minX, maxX, minY, maxY);

    float leftLimit = playerClampMarginX_;
    float rightLimit = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - playerClampMarginX_;

    float topLimit = playerClampMarginY_;
    float bottomLimit = static_cast<float>(WinApp::GetInstance()->GetClientHeight()) - playerClampMarginY_;

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

// 弾を発射する関数
void Player::FireBullet(const Camera& activeCamera)
{
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
    std::unique_ptr<PlayerBullet> bullet = CreateBullet(shotSpeed);

    bullet->Initialize(bulletModel_);
    bullet->SetCamera(camera_);

    Vector3 muzzlePosition = CalculateMuzzlePosition();
    EffectManager::GetInstance()->PlayEffect("ShotBullet", muzzlePosition);

    bullet->SetTranslate(muzzlePosition);

    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);

    Vector3 aimPoint = ResolveAimPoint(aimRay, muzzlePosition);

#ifdef _DEBUG
    drawDebugLines_ = true;
    debugAimRayOrigin_ = aimRay.origin;
    debugAimPoint_ = aimPoint;
    debugMuzzlePosition_ = muzzlePosition;

    // 描画用カメラでの逆投影レイを作成してエイムポイントを計算する（デバッグ比較用）
    Ray drawRay {};
    CreateAimRay(drawRay, *camera_);
    Vector3 drawAimPoint = ResolveAimPoint(drawRay, muzzlePosition);
    debugDrawRayOrigin_ = drawRay.origin;
    debugDrawAimPoint_ = drawAimPoint;
#endif

    Vector3 bulletDirection = Normalize(aimPoint - muzzlePosition);

    // プレイヤーのワールド移動速度 (慣性) を進行方向とスピードから計算
    Vector3 worldPlayerVelocity = railForward_ * velocity_.z;

    Vector3 bulletVelocity;
    bulletVelocity.x = bulletDirection.x * shotSpeed + worldPlayerVelocity.x;
    bulletVelocity.y = bulletDirection.y * shotSpeed + worldPlayerVelocity.y;
    bulletVelocity.z = bulletDirection.z * shotSpeed + worldPlayerVelocity.z;

    bullet->SetVelocity(bulletVelocity);

    // 描画前にワールド行列を最新座標に同期する
    bullet->Update();

    bullets_.push_back(std::move(bullet));
}

Vector3 Player::CalculateMuzzlePosition() const
{
    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Vector3 localMuzzle = { 0.0f, bulletSpawnOffsetY_, bulletSpawnOffsetZ_ };
    Vector3 muzzlePosition = MatrixMath::Transform(localMuzzle, worldMatrix);

    return muzzlePosition;
}

void Player::CreateAimRay(Ray& aimRay, const Camera& activeCamera) const
{
    float mouseX = aimScreenPosition_.x;
    float mouseY = aimScreenPosition_.y;

    float screenWidth = static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    float screenHeight = static_cast<float>(WinApp::GetInstance()->GetClientHeight());

    float ndcX = (2.0f * mouseX / screenWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / screenHeight);

    Matrix4x4 inverseProjection = MatrixMath::Inverse(activeCamera.GetProjectionMatrix());
    Matrix4x4 inverseView = MatrixMath::Inverse(activeCamera.GetViewMatrix());

    Vector3 nearPoint = { ndcX, ndcY, 0.0f };
    Vector3 farPoint = { ndcX, ndcY, 1.0f };

    nearPoint = MatrixMath::Transform(nearPoint, inverseProjection);
    farPoint = MatrixMath::Transform(farPoint, inverseProjection);

    nearPoint = MatrixMath::Transform(nearPoint, inverseView);
    farPoint = MatrixMath::Transform(farPoint, inverseView);

    aimRay.origin = nearPoint;
    aimRay.direction = Normalize(farPoint - nearPoint);
}

Vector3 Player::CreateConvergencePoint(const Ray& aimRay) const
{
    return aimRay.origin + aimRay.direction * kAimConvergenceDistance;
}

Vector3 Player::ResolveAimPoint(
    const Ray& aimRay,
    const Vector3& muzzlePosition) const
{
    Vector3 convergencePoint = CreateConvergencePoint(aimRay);
    Vector3 aimPoint = convergencePoint;
    RaycastHit hit {};

    // レティクルレイが敵の当たり判定に重なっている場合、
    // 複雑な角度制限は一切無視して、無条件で弾道を敵の中心へ100%吸い付かせます
    if (CollisionManager::GetInstance()->Raycast(aimRay, hit)) {
        aimPoint = hit.enemy->GetPosition();
    }

    return aimPoint;
}



std::unique_ptr<PlayerBullet> Player::CreateBullet(float& shotSpeed)
{
    switch (currentWeapon_) {

    case kWeaponMissileBullet: {
        std::unique_ptr<MissileBullet> missileBullet = std::make_unique<MissileBullet>();

        shotSpeed = missileBullet->GetSpeed() / 60.0f;

        return missileBullet;
    }

    case kWeaponNormalBullet:
    default: {
        shotSpeed = bulletSpeed_;

        return std::make_unique<NormalBullet>();
    }
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
    for (std::unique_ptr<PlayerBullet>& bullet : bullets_) {
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
    Vector3 nextRailOffset = railOffset_;

    if (input->IsKeyPressed(DIK_A)) {
        nextRailOffset.x -= moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_D)) {
        nextRailOffset.x += moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_W)) {
        nextRailOffset.y += moveSpeed_;
    }

    if (input->IsKeyPressed(DIK_S)) {
        nextRailOffset.y -= moveSpeed_;
    }

    nextRailOffset.z = 0.0f;
    railOffset_ = ClampRailOffsetToScreen(nextRailOffset);
}

Vector3 Player::CalculateRailWorldPosition(const Vector3& railOffset) const
{
    Vector3 position = railBasePosition_;
    position += railRight_ * railOffset.x;
    position += railUp_ * railOffset.y;
    return position;
}

Vector3 Player::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    Vector3 correctedRailOffset = railOffset;
    correctedRailOffset.z = 0.0f;

    if (camera_ == nullptr) {
        return correctedRailOffset;
    }

    const int correctionCount = 3;
    for (int correctionIndex = 0; correctionIndex < correctionCount; ++correctionIndex) {
        Vector2 screenCorrection = CalculateScreenCorrection(correctedRailOffset);

        if (std::fabs(screenCorrection.x) < 0.01f && std::fabs(screenCorrection.y) < 0.01f) {
            break;
        }

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

        float determinant = rightScreenX * upScreenY - rightScreenY * upScreenX;

        if (std::fabs(determinant) > 0.0001f) {
            float offsetX = (screenCorrection.x * upScreenY - screenCorrection.y * upScreenX) / determinant;
            float offsetY = (rightScreenX * screenCorrection.y - rightScreenY * screenCorrection.x) / determinant;

            correctedRailOffset.x += offsetX;
            correctedRailOffset.y += offsetY;
        } else {
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

        correctedRailOffset.z = 0.0f;
    }

    return correctedRailOffset;
}

void Player::UpdateScreenBounds(const Vector3& worldPosition,float& minX,float& maxX,float& minY,float& maxY) const
{
    if (camera_ == nullptr) {
        return;
    }

    Vector2 screenPosition = camera_->WorldToScreen(worldPosition);

    if (screenPosition.x < minX) {
        minX = screenPosition.x;
    }

    if (screenPosition.x > maxX) {
        maxX = screenPosition.x;
    }

    if (screenPosition.y < minY) {
        minY = screenPosition.y;
    }

    if (screenPosition.y > maxY) {
        maxY = screenPosition.y;
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

    ImGui::Text("Rail Offset");

    ImGui::Text("X : %.2f", railOffset_.x);
    ImGui::Text("Y : %.2f", railOffset_.y);
    ImGui::Text("Z : %.2f", railOffset_.z);

    ImGui::Separator();

    ImGui::Text("Rail Frame");

    ImGui::Text("Right X : %.2f Y : %.2f Z : %.2f", railRight_.x, railRight_.y, railRight_.z);
    ImGui::Text("Up    X : %.2f Y : %.2f Z : %.2f", railUp_.x, railUp_.y, railUp_.z);

    ImGui::Separator();

    ImGui::Text("Velocity");

    ImGui::Text("X : %.2f", velocity_.x);
    ImGui::Text("Y : %.2f", velocity_.y);
    ImGui::Text("Z : %.2f", velocity_.z);
    ImGui::Separator();

    ImGui::Text("Weapon No : %d", currentWeapon_);
    ImGui::Text("Weapon Name : %s", GetCurrentWeaponName());
    ImGui::Separator();

    ImGui::Text("HP : %d / %d", currentHp_, maxHp_);
    ImGui::Text("Invincible : %d", invincibleTimer_);
    ImGui::Separator();

    ImGui::Text("Screen Clamp Margin");

    ImGui::DragFloat(
        "ScreenMarginX",
        &playerClampMarginX_,
        0.1f);

    ImGui::DragFloat(
        "ScreenMarginY",
        &playerClampMarginY_,
        0.1f);

    ImGui::DragFloat(
        "BoundsHalfWidth",
        &playerBoundsHalfWidth_,
        0.1f);

    ImGui::DragFloat(
        "BoundsHalfHeight",
        &playerBoundsHalfHeight_,
        0.1f);
    ImGui::End();
}

#endif // _DEBUG
