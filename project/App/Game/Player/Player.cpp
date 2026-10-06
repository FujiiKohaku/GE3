#include "Player.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include <cassert>
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
    movementController_.Initialize();

    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);

    aimController_.Initialize();
}

void Player::Update()
{
    if (object_ == nullptr || TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        return;
    }
    if (!health_.IsAlive()) {
        UpdateDeath();
        return;
    }
    Input* input = Input::GetInstance();
    if (input == nullptr) {
        return;
    }
    if (camera_ != nullptr) {
        aimController_.UpdateSmoothedAimDistance(*camera_, TimeManager::GetInstance()->GetDeltaTime());
    }
    health_.UpdateInvincibility();
    if (debugCameraController_ != nullptr) {
        isDebugMode_ = debugCameraController_->GetDebugMode();
    }

    movementController_.UpdateBoost(input, isDebugMode_);
    weaponController_.UpdateInput(input, isDebugMode_);
    if (!isDebugMode_) {
        aimController_.UpdateMouseAim();
        movementController_.UpdateMovement(input);
        aimController_.ClampAimScreenPosition();
    }
    movementController_.ApplyRailPosition();
    ApplyTransform();
    weaponController_.UpdateBullets();
    weaponController_.RemoveDeadBullets();
    object_->Update();
#ifdef _DEBUG
    weaponController_.DrawAimDebugLines();
#endif
}

void Player::UpdateDeath()
{
    if (health_.IsFalling()) {
        health_.UpdateDeathAnimation(transform_, TimeManager::GetInstance()->GetDeltaTime());
        ApplyTransform();
    }
    weaponController_.UpdateBullets();
    weaponController_.RemoveDeadBullets();
    object_->Update();
}

void Player::SetCamera(Camera* camera)
{
    camera_ = camera;
    movementController_.SetCamera(camera);
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
    if (object_ != nullptr) {
        ApplyTransform();
        object_->Update();
    }
}

void Player::SetRotate(const Vector3& rotate)
{
    transform_.rotate.x = rotate.x;
    transform_.rotate.y = rotate.y;
    if (!movementController_.IsRolling()) {
        transform_.rotate.z = rotate.z;
    }
}

void Player::SetRailFrame(const Vector3& railBasePosition, const Vector3& railRight,
    const Vector3& railUp, const Vector3& railForward)
{
    movementController_.SetRailFrame(railBasePosition, railRight, railUp, railForward);
}

void Player::EnableAllRangeMode(float areaRadius, float minHeight, float maxHeight)
{
    movementController_.EnableAllRangeMode(areaRadius, minHeight, maxHeight);
}

Vector3 Player::GetAutomaticWorldVelocity() const
{
    if (health_.IsDead()) {
        return {};
    }
    return movementController_.GetAutomaticWorldVelocity();
}

bool Player::ApplyDamage(int damage)
{
    const bool hasAppliedDamage = health_.ApplyDamage(damage, movementController_.IsRolling());
    if (hasAppliedDamage && health_.IsDead()) {
        movementController_.Stop();
        weaponController_.ResetHomingLock();
    }
    return hasAppliedDamage;
}

void Player::Draw()
{
    if (object_ == nullptr) {
        return;
    }
    weaponController_.DrawBullets();
    if (health_.ShouldDraw()) {
        object_->Draw();
    }
}

void Player::DrawShadow(ShadowMapRenderer& renderer)
{
    if (object_ != nullptr && health_.ShouldDraw()) {
        object_->DrawShadow(renderer);
    }
}

void Player::ApplyTransform()
{
    object_->SetScale(transform_.scale);
    object_->SetRotate(transform_.rotate);
    object_->SetTranslate(transform_.translate);
}

void Player::DrawImGui()
{
#ifdef _DEBUG
    ImGui::Begin("Player Controls");
    ImGui::Text("HP: %d / %d", health_.GetCurrentHp(), health_.GetMaxHp());
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
