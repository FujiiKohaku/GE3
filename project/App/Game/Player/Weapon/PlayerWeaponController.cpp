#include "App/Game/Player/Weapon/PlayerWeaponController.h"
#include "App/Game/Player/Aim/PlayerAimController.h"
#include "App/Game/Player/Movement/PlayerMovementController.h"
#include "App/Game/Player/Health/PlayerHealth.h"
#include "App/Game/Player/Bullet/MissileBullet.h"
#include "App/Game/Player/Bullet/HomingMissileBullet.h"
#include "App/Game/Player/Bullet/NormalBullet.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/audio/SoundManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include <algorithm>

void PlayerWeaponController::Initialize()
{
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

void PlayerWeaponController::UpdateInput(Input* input, bool isDebugMode)
{
    if (!isDebugMode) {
        UpdateWeaponSwitch(input);
    }
    const bool isFireHeld = input->IsKeyPressed(DIK_SPACE) || input->IsMousePressed(0);
    if (missileFireCooldownSeconds_ + 0.000001f < kMissileFireIntervalSeconds) {
        missileFireCooldownSeconds_ = (std::min)(kMissileFireIntervalSeconds,
            missileFireCooldownSeconds_ + TimeManager::GetInstance()->GetDeltaTime());
    }
    // クールダウンを進めてから、発射可否を判定する。
    UpdateHomingFireInput(isFireHeld, isDebugMode);
    UpdateWeaponHeat(isFireHeld, isDebugMode);
    UpdateFireInput(input, isDebugMode);
}

void PlayerWeaponController::UpdateHomingFireInput(bool isFireHeld, bool isDebugMode)
{
    const bool isEnabled = !isDebugMode && IsHomingMissileSelected();
    // 画面上の照準位置を、ホーミングミサイルのロック対象を選ぶ基準に使う。
    if (homingLock_.UpdateFireInput(isFireHeld, isEnabled, camera_, transform_.translate,
        movementController_.GetFlightForward(), aimController_.GetAimScreenPosition())) {
        if (FireBullet(*camera_)) {
            homingLock_.ClearTargets();
        }
    }
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

void PlayerWeaponController::UpdateWeaponHeat(bool isHomingFireHeld, bool isDebugMode)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    const bool isHeatWeaponSelected = currentWeapon_ == kWeaponNormalBullet ||
        currentWeapon_ == kWeaponMinigun;
    const bool isHeatFireHeld = !isDebugMode && camera_ != nullptr &&
        isHeatWeaponSelected && isHomingFireHeld;
    weaponHeat_.Update(deltaTimeSeconds, isHeatFireHeld);
}

void PlayerWeaponController::UpdateFireInput(Input* input, bool isDebugMode)
{
    // 長押し連射。熱量は実際の発射時に加算する。
    if (!isDebugMode && (input->IsKeyPressed(DIK_SPACE) || input->IsMousePressed(0))) {
        // ミニガンは高速連射、通常弾はそれより遅い連射にする
        if (currentWeapon_ == kWeaponMinigun) {
            minigunFireCooldownSeconds_ += TimeManager::GetInstance()->GetDeltaTime();
            while (minigunFireCooldownSeconds_ + 0.000001f >= kMinigunFireIntervalSeconds) {
                minigunFireCooldownSeconds_ = (std::max)(0.0f, minigunFireCooldownSeconds_ - kMinigunFireIntervalSeconds);
                if (camera_) {
                    FireBullet(*camera_);
                }
            }
        } else if (currentWeapon_ == kWeaponNormalBullet) {
            normalFireCooldownSeconds_ += TimeManager::GetInstance()->GetDeltaTime();
            while (normalFireCooldownSeconds_ + 0.000001f >= kNormalFireIntervalSeconds) {
                normalFireCooldownSeconds_ = (std::max)(0.0f, normalFireCooldownSeconds_ - kNormalFireIntervalSeconds);
                if (camera_) {
                    FireBullet(*camera_);
                }
            }
        }
    }

    if (!isDebugMode && currentWeapon_ == kWeaponMissileBullet &&
        (input->IsKeyTrigger(DIK_SPACE) || input->IsMouseTrigger(0)) &&
        camera_ != nullptr) {
        FireBullet(*camera_);
    }
}

bool PlayerWeaponController::FireBullet(const Camera& activeCamera)
{
    const bool isHeatWeapon = currentWeapon_ == kWeaponNormalBullet ||
        currentWeapon_ == kWeaponMinigun;
    if (health_.IsDead() || (isHeatWeapon && weaponHeat_.IsOverheated())) {
        return false;
    }
    if (bulletModel_ == nullptr || camera_ == nullptr) {
        return false;
    }

    if (currentWeapon_ == kWeaponMissileBullet ||
        currentWeapon_ == kWeaponHomingMissile) {
        if (missileFireCooldownSeconds_ + 0.000001f < kMissileFireIntervalSeconds) {
            return false;
        }
        missileFireCooldownSeconds_ = 0;
    }

    if (currentWeapon_ == kWeaponHomingMissile &&
        !homingLock_.GetLockedTargets().empty()) {
        SoundManager::GetInstance()->Play("MissileShot");
        for (BaseEnemy* target : homingLock_.GetLockedTargets()) {
            FireSingleBullet(activeCamera, target);
        }
        return true;
    }

    switch (currentWeapon_) {
    case kWeaponMissileBullet:
    case kWeaponHomingMissile:
        SoundManager::GetInstance()->Play("MissileShot");
        break;
    case kWeaponMinigun:
        SoundManager::GetInstance()->Play("MinigunShot");
        break;
    default:
        SoundManager::GetInstance()->Play("NormalShot");
        break;
    }
    FireSingleBullet(activeCamera, nullptr);
    if (isHeatWeapon) {
        weaponHeat_.AddShotHeat(currentWeapon_ == kWeaponMinigun);
    }
    return true;
}

void PlayerWeaponController::FireSingleBullet(const Camera& activeCamera, BaseEnemy* homingTarget)
{

    float shotSpeed = bulletSpeed_;
    std::unique_ptr<PlayerBullet> bullet = CreateBullet(shotSpeed);

    bullet->Initialize(bulletModel_);
    bullet->SetCamera(camera_);
    if (currentWeapon_ == kWeaponMinigun) {
        bullet->SetDamage(kMinigunDamage);
    } else if (currentWeapon_ == kWeaponNormalBullet) {
        bullet->SetDamage(kNormalBulletDamage);
    }

    Vector3 muzzlePosition = CalculateMuzzlePosition();
    EffectManager::GetInstance()->PlayEffect("ShotBullet", muzzlePosition);

    bullet->SetTranslate(muzzlePosition);

    Ray aimRay {};
    // 発射処理で、画面上の照準を3D空間の方向へ変換する。
    aimController_.CreateAimRay(aimRay, activeCamera);

    // 銃口から弾を向ける位置を、補間済みの照準距離から求める。
    Vector3 aimPoint = aimController_.ResolveAimPoint(aimRay);

    if (HomingMissileBullet* missile = dynamic_cast<HomingMissileBullet*>(bullet.get())) {
        missile->SetTarget(homingTarget, homingLock_.GetTargets());
    }

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
}

Vector3 PlayerWeaponController::CalculateMuzzlePosition() const
{
    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Vector3 localMuzzle = { 0.0f, bulletSpawnOffsetY_, bulletSpawnOffsetZ_ };
    return MatrixMath::Transform(localMuzzle, worldMatrix);
}

std::unique_ptr<PlayerBullet> PlayerWeaponController::CreateBullet(float& shotSpeed)
{
    switch (currentWeapon_) {

    case kWeaponMissileBullet: {
        std::unique_ptr<MissileBullet> missileBullet =
            std::make_unique<MissileBullet>();
        shotSpeed = missileBullet->GetSpeed() / 60.0f;
        return missileBullet;
    }

    case kWeaponHomingMissile: {
        std::unique_ptr<HomingMissileBullet> missileBullet =
            std::make_unique<HomingMissileBullet>();
        shotSpeed = missileBullet->GetSpeed() / 60.0f;
        return missileBullet;
    }

    case kWeaponMinigun:
        [[fallthrough]];
    case kWeaponNormalBullet:
        [[fallthrough]];
    default: {
        shotSpeed = bulletSpeed_;
        return std::make_unique<NormalBullet>();
    }
    }
}

void PlayerWeaponController::UpdateWeaponSwitch(Input* input)
{
    if (input->GetMouseWheel() > 0) {
        currentWeapon_ = (currentWeapon_ + 1) % kWeaponCount;
    }

    if (input->GetMouseWheel() < 0) {
        currentWeapon_ = (currentWeapon_ + kWeaponCount - 1) % kWeaponCount;
    }

    if (input->IsKeyTrigger(DIK_1)) currentWeapon_ = kWeaponNormalBullet;
    if (input->IsKeyTrigger(DIK_2)) currentWeapon_ = kWeaponMissileBullet;
    if (input->IsKeyTrigger(DIK_3)) currentWeapon_ = kWeaponHomingMissile;
    if (input->IsKeyTrigger(DIK_4)) currentWeapon_ = kWeaponMinigun;
}

const char* PlayerWeaponController::GetCurrentWeaponName() const
{
    switch (currentWeapon_) {
    case kWeaponMissileBullet:
        return "Missile";
    case kWeaponHomingMissile:
        return "Homing Missile";
    case kWeaponMinigun:
        return "Minigun";
    case kWeaponNormalBullet:
    default:
        return "Normal";
    }
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
