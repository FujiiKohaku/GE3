#include "App/Game/Player/Weapon/PlayerWeaponController.h"
#include "App/Game/Player/Aim/PlayerAimController.h"
#include "App/Game/Player/Movement/PlayerMovementController.h"
#include "App/Game/Player/Health/PlayerHealth.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include <algorithm>

void PlayerWeaponController::Initialize()
{
    shotsFired_ = 0;
    ModelManager::GetInstance()->Load("Debug/block/block.obj");
    bulletModel_ = ModelManager::GetInstance()->Load("Debug/block/block.obj");
}

void PlayerWeaponController::SetCamera(Camera* camera)
{
    camera_ = camera;
    for (const auto& bullet : bullets_) {
        bullet->SetCamera(camera);
    }
}

PlayerWeaponContext PlayerWeaponController::MakeWeaponContext(const Camera* activeCamera)
{
    return { weaponHeat_, homingLock_, missileFireCooldownSeconds_, camera_,
        transform_.translate, movementController_.GetFlightForward(), aimController_.GetAimScreenPosition(),
        !health_.IsDead() && bulletModel_ != nullptr && camera_ != nullptr,
        [this]() { return camera_ && FireBullet(*camera_); },
        [this, activeCamera](BaseEnemy* target) {
            const Camera* shotCamera = activeCamera ? activeCamera : camera_;
            if (shotCamera) FireSingleBullet(*shotCamera, target);
        } };
}

void PlayerWeaponController::UpdateInput(Input* input, bool isDebugMode)
{
    if (input->IsPrimaryFireBlocked()) homingLock_.Reset();
    if (!isDebugMode) UpdateWeaponSwitch(input);
    const float dt = TimeManager::GetInstance()->GetDeltaTime();
    missileFireCooldownSeconds_ = (std::min)(kMissileFireIntervalSeconds, missileFireCooldownSeconds_ + dt);
    BasePlayerWeapon& weapon = GetCurrentWeapon();
    if (isDebugMode || !weapon.IsHoming()) homingLock_.Reset();
    auto context = MakeWeaponContext();
    context.canFire = context.canFire && !isDebugMode;
    // ミサイル選択中にも共通の熱ゲージは冷却を続ける。
    weaponHeat_.Update(dt, context.canFire && weapon.UsesHeat() && input->IsPrimaryFirePressed());
    weapon.Update(context, input->IsPrimaryFirePressed(), input->IsPrimaryFireTrigger(), dt);
}
void PlayerWeaponController::GetHomingLockPositions(std::vector<Vector3>& positions) const
{
    positions.clear();
    if (IsHomingMissileSelected()) {
        homingLock_.GetPositions(positions);
    }
}

void PlayerWeaponController::DrawBullets()
{
    for (const auto& bullet : bullets_) {
        bullet->Draw();
    }
}

void PlayerWeaponController::SubmitRaytracing(DxrRenderer& renderer) {
    for (const auto& bullet : bullets_) {
        if (bullet) { bullet->SubmitRaytracing(renderer); }
    }
}

bool PlayerWeaponController::FireBullet(const Camera& activeCamera)
{
    auto context = MakeWeaponContext(&activeCamera);
    return GetCurrentWeapon().Fire(context);
}
void PlayerWeaponController::FireSingleBullet(const Camera& activeCamera, BaseEnemy* homingTarget)
{

    float shotSpeed = bulletSpeed_;
    auto context = MakeWeaponContext(&activeCamera);
    std::unique_ptr<PlayerBullet> bullet = GetCurrentWeapon().CreateBullet(
        bulletModel_, context, homingTarget, shotSpeed);
    bullet->SetCamera(camera_);
    Vector3 muzzlePosition = CalculateMuzzlePosition();
    EffectManager::GetInstance()->PlayEffect("ShotBullet", muzzlePosition);

    bullet->SetTranslate(muzzlePosition);

    Ray aimRay {};
    // 発射処理で、画面上の照準を3D空間の方向へ変換する。
    aimController_.CreateAimRay(aimRay, activeCamera);

    // 銃口から弾を向ける位置を、補間済みの照準距離から求める。
    Vector3 aimPoint = aimController_.ResolveAimPoint(aimRay);


#ifdef _DEBUG
    shouldDrawDebugLines_ = true;
    debugAimRayOrigin_ = aimRay.origin;
    debugAimPoint_ = aimPoint;
    debugMuzzlePosition_ = muzzlePosition;

    Ray drawRay {};
    // デバッグ描画用に照準レイと狙い位置を求め、発射方向との関係を表示する。
    aimController_.CreateAimRay(drawRay, *camera_);
    Vector3 drawAimPoint = aimController_.ResolveAimPoint(drawRay);
    debugDrawRayOrigin_ = drawRay.origin;
    debugDrawAimPoint_ = drawAimPoint;
#endif

    Vector3 bulletDirection = Normalize(aimPoint - muzzlePosition);
    Vector3 worldPlayerVelocity = movementController_.GetWorldVelocity();

    Vector3 bulletVelocity;
    bulletVelocity.x = bulletDirection.x * shotSpeed + worldPlayerVelocity.x;
    bulletVelocity.y = bulletDirection.y * shotSpeed + worldPlayerVelocity.y;
    bulletVelocity.z = bulletDirection.z * shotSpeed + worldPlayerVelocity.z;

    bullet->SetVelocity(bulletVelocity);

    bullets_.push_back(std::move(bullet));
    ++shotsFired_;
}

Vector3 PlayerWeaponController::CalculateMuzzlePosition() const
{
    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Vector3 localMuzzle = { 0.0f, bulletSpawnOffsetY_, bulletSpawnOffsetZ_ };
    return MatrixMath::Transform(localMuzzle, worldMatrix);
}

void PlayerWeaponController::UpdateWeaponSwitch(Input* input)
{
    if (input->GetMouseWheel() > 0 || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_RIGHT)) {
        currentWeapon_ = (currentWeapon_ + 1) % kWeaponCount;
    }

    if (input->GetMouseWheel() < 0 || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_LEFT)) {
        currentWeapon_ = (currentWeapon_ + kWeaponCount - 1) % kWeaponCount;
    }

    if (input->IsKeyTrigger(DIK_1)) currentWeapon_ = kWeaponNormalBullet;
    if (input->IsKeyTrigger(DIK_2)) currentWeapon_ = kWeaponMissileBullet;
    if (input->IsKeyTrigger(DIK_3)) currentWeapon_ = kWeaponHomingMissile;
    if (input->IsKeyTrigger(DIK_4)) currentWeapon_ = kWeaponMinigun;
}

const char* PlayerWeaponController::GetCurrentWeaponName() const
{
    return GetCurrentWeapon().GetName();
}
void PlayerWeaponController::UpdateBullets()
{
    for (std::unique_ptr<PlayerBullet>& bullet : bullets_) {
        bullet->Update();
    }
}

void PlayerWeaponController::RemoveDeadBullets()
{
    for (uint32_t i = 0; i < bullets_.size();) {
        if (!bullets_[i]->IsAlive()) {
            bullets_.erase(bullets_.begin() + i);
        } else {
            ++i;
        }
    }

}

#ifdef _DEBUG
void PlayerWeaponController::DrawAimDebugLines()
{
    if (shouldDrawDebugLines_) {
        // 1. AimCameraから飛ぶRay (青)
        DebugRenderer::GetInstance()->AddLine(debugAimRayOrigin_, debugAimPoint_, { 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f);

        // 2. Playerのマズルから飛ぶ実際の弾の進行方向 (赤)
        DebugRenderer::GetInstance()->AddLine(debugMuzzlePosition_, debugAimPoint_, { 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f);

        // 3. 描画用Cameraから飛ぶRay (黄色 - デバッグ比較専用)
        DebugRenderer::GetInstance()->AddLine(debugDrawRayOrigin_, debugDrawAimPoint_, { 1.0f, 1.0f, 0.0f, 1.0f }, 3.0f);
    }
}
#endif
