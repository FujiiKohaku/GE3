#include "App/Game/Player/Player.h"
#include "App/Game/Player/Bullet/MissileBullet.h"
#include "App/Game/Player/Bullet/HomingMissileBullet.h"
#include "App/Game/Player/Bullet/NormalBullet.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"
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
    object_->SetEnableLighting(true);
    object_->SetModel(model);

    if (camera_ != nullptr) {
        object_->SetCamera(camera_);
    }
    ModelManager::GetInstance()->Load("Debug/block/block.obj");
    bulletModel_ = ModelManager::GetInstance()->Load("Debug/block/block.obj");

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

    if (TimeManager::GetInstance()->GetDeltaTime() <= 0.0f) {
        return;
    }

    if (deathState_ != DeathState::Alive) {
        UpdateDeathAnimation();
        UpdateBullets();
        RemoveDeadBullets();
        object_->Update();
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

    isBoosting_ = input->IsKeyPressed(DIK_LSHIFT);
    velocity_.z = normalMaxSpeed_;
    moveSpeed_ = normalAcceleration_;
    if (isBoosting_) {
        velocity_.z = boostMaxSpeed_;
        moveSpeed_ = boostAcceleration_;
    }

    UpdateWeaponSwitch(input);
    const bool isHomingFireHeld =
        input->IsKeyPressed(DIK_SPACE) || input->IsMousePressed(0);
    if (currentWeapon_ == kWeaponHomingMissile) {
        if (isHomingFireHeld && !wasHomingFireHeld_) {
            lockedHomingTargets_.clear();
        }
        UpdateHomingTarget(isHomingFireHeld);
        if (!isHomingFireHeld && wasHomingFireHeld_ && camera_ != nullptr) {
            FireBullet(*camera_);
            lockedHomingTargets_.clear();
        }
        wasHomingFireHeld_ = isHomingFireHeld;
    } else {
        wasHomingFireHeld_ = false;
        lockedHomingTargets_.clear();
    }
    if (missileFireCooldownFrames_ < kMissileFireIntervalFrames) {
        ++missileFireCooldownFrames_;
    }

    // ミニガンの熱気自然冷却
    minigunHeat_ -= 1.0f / 180.0f;
    if (minigunHeat_ < 0.0f) minigunHeat_ = 0.0f;

    // 攻撃ボタン長押しで熱気蓄積 ＆ ミニガン超高速連射
    if (input->IsKeyPressed(DIK_SPACE) || input->IsMousePressed(0)) {
        minigunHeat_ += 1.0f / 80.0f;
        if (minigunHeat_ > 1.0f) minigunHeat_ = 1.0f;

        // ミニガンは高速連射、通常弾はそれより遅い連射にする
        if (currentWeapon_ == kWeaponMinigun) {
            minigunFireCooldown_++;
            if (minigunFireCooldown_ >= kMinigunFireIntervalFrames) {
                minigunFireCooldown_ = 0;
                if (camera_) {
                    FireBullet(*camera_);
                }
            }
        } else if (currentWeapon_ == kWeaponNormalBullet) {
            normalFireCooldown_++;
            if (normalFireCooldown_ >= kNormalFireIntervalFrames) {
                normalFireCooldown_ = 0;
                if (camera_) {
                    FireBullet(*camera_);
                }
            }
        }
    }

    if (currentWeapon_ == kWeaponMissileBullet &&
        (input->IsKeyTrigger(DIK_SPACE) || input->IsMouseTrigger(0)) &&
        camera_ != nullptr) {
        FireBullet(*camera_);
    }

    // デバッグカメラモードでないときは、マウスで照準を動かし、キーボードでプレイヤーを動かす
    if (!isDebugMode) {
        UpdateMouseAim();
        UpdateRolling(input);
        if (controlMode_ == ControlMode::StarFox) {
            UpdateStarFoxMove();
        } else {
            UpdateKeyboardMove(input);
        }
        ClampAimScreenPosition();
    }

    transform_.translate = CalculateRailWorldPosition(railOffset_);

    // transform反映
    ApplyTransform();
    // 弾更新
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

void Player::SetEnableLighting(bool enable)
{
    if (object_ != nullptr) {
        object_->SetEnableLighting(enable);
    }
}

void Player::Draw()
{
    if (object_ == nullptr) {
        return;
    }
    for (std::unique_ptr<PlayerBullet>& bullet : bullets_) {
        bullet->Draw();
    }
    const bool isVisibleWhileAlive =
        invincibleTimer_ <= 0 || (invincibleTimer_ / 4) % 2 == 0;
    const bool shouldDrawPlayer =
        deathState_ == DeathState::Falling ||
        (deathState_ == DeathState::Alive && isVisibleWhileAlive);
    if (shouldDrawPlayer) {
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
    if (damage <= 0 || invincibleTimer_ > 0 || isRolling_ || currentHp_ <= 0) {
        return false;
    }

    currentHp_ -= damage;
    if (currentHp_ < 0) {
        currentHp_ = 0;
    }
    if (currentHp_ == 0) {
        deathState_ = DeathState::Falling;
        deathTimer_ = 0.0f;
        deathFallVelocity_ = 0.0f;
        isBoosting_ = false;
        isRolling_ = false;
        lockedHomingTargets_.clear();
        wasHomingFireHeld_ = false;
    }
    invincibleTimer_ = kInvincibleFrames;
    return true;
}

void Player::UpdateDeathAnimation()
{
    if (deathState_ != DeathState::Falling) {
        return;
    }

    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    deathTimer_ += deltaTime;
    deathFallVelocity_ += 18.0f * deltaTime;
    transform_.translate.y -= deathFallVelocity_ * deltaTime;
    transform_.rotate.x += 1.4f * deltaTime;
    transform_.rotate.z += 3.2f * deltaTime;
    ApplyTransform();

    if (deathTimer_ >= kDeathFallDuration) {
        deathState_ = DeathState::Exploded;
    }
}

bool Player::Heal(int amount)
{
    if (amount <= 0 || currentHp_ <= 0 || currentHp_ >= maxHp_) {
        return false;
    }

    currentHp_ += amount;
    if (currentHp_ > maxHp_) {
        currentHp_ = maxHp_;
    }

    return true;
}

// 照準の画面上の位置を制限する関数
void Player::ClampAimScreenPosition()
{
    float halfAimSize = 64.0f;

    if (aimScreenPosition_.x < halfAimSize) {
        aimScreenPosition_.x = halfAimSize;
    }

    if (aimScreenPosition_.x > static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - halfAimSize) {
        aimScreenPosition_.x = static_cast<float>(WinApp::GetInstance()->GetClientWidth()) - halfAimSize;
    }

    if (aimScreenPosition_.y < halfAimSize) {
        aimScreenPosition_.y = halfAimSize;
    }

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
    if (bulletModel_ == nullptr || camera_ == nullptr) {
        return;
    }

    if (currentWeapon_ == kWeaponMissileBullet ||
        currentWeapon_ == kWeaponHomingMissile) {
        if (missileFireCooldownFrames_ < kMissileFireIntervalFrames) {
            return;
        }
        missileFireCooldownFrames_ = 0;
    }

    if (currentWeapon_ == kWeaponHomingMissile &&
        !lockedHomingTargets_.empty()) {
        for (BaseEnemy* target : lockedHomingTargets_) {
            FireSingleBullet(activeCamera, target);
        }
        return;
    }

    FireSingleBullet(activeCamera, nullptr);
}

void Player::FireSingleBullet(const Camera& activeCamera, BaseEnemy* homingTarget)
{

    float shotSpeed = bulletSpeed_;
    std::unique_ptr<PlayerBullet> bullet = CreateBullet(shotSpeed);

    bullet->Initialize(bulletModel_);
    bullet->SetCamera(camera_);
    if (currentWeapon_ == kWeaponMinigun) {
        bullet->SetDamage(kMinigunDamage);
        minigunFireCooldown_ = 0;
    } else if (currentWeapon_ == kWeaponNormalBullet) {
        bullet->SetDamage(kNormalBulletDamage);
        normalFireCooldown_ = 0;
    }

    Vector3 muzzlePosition = CalculateMuzzlePosition();
    EffectManager::GetInstance()->PlayEffect("ShotBullet", muzzlePosition);

    bullet->SetTranslate(muzzlePosition);

    Ray aimRay {};
    CreateAimRay(aimRay, activeCamera);

    Vector3 aimPoint = ResolveAimPoint(aimRay, muzzlePosition);

    if (HomingMissileBullet* missile = dynamic_cast<HomingMissileBullet*>(bullet.get())) {
        missile->SetTarget(homingTarget, homingTargets_);
    }

#ifdef _DEBUG
    drawDebugLines_ = true;
    debugAimRayOrigin_ = aimRay.origin;
    debugAimPoint_ = aimPoint;
    debugMuzzlePosition_ = muzzlePosition;

    Ray drawRay {};
    CreateAimRay(drawRay, *camera_);
    Vector3 drawAimPoint = ResolveAimPoint(drawRay, muzzlePosition);
    debugDrawRayOrigin_ = drawRay.origin;
    debugDrawAimPoint_ = drawAimPoint;
#endif

    Vector3 bulletDirection = Normalize(aimPoint - muzzlePosition);
    Vector3 worldPlayerVelocity = railForward_ * velocity_.z;

    Vector3 bulletVelocity;
    bulletVelocity.x = bulletDirection.x * shotSpeed + worldPlayerVelocity.x;
    bulletVelocity.y = bulletDirection.y * shotSpeed + worldPlayerVelocity.y;
    bulletVelocity.z = bulletDirection.z * shotSpeed + worldPlayerVelocity.z;

    bullet->SetVelocity(bulletVelocity);
    bullet->Update();

    bullets_.push_back(std::move(bullet));
}

Vector3 Player::CalculateMuzzlePosition() const
{
    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Vector3 localMuzzle = { 0.0f, bulletSpawnOffsetY_, bulletSpawnOffsetZ_ };
    return MatrixMath::Transform(localMuzzle, worldMatrix);
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

void Player::SetMouseSensitivity(float sensitivity)
{
    mouseSensitivity_ = std::clamp(sensitivity, 0.5f, 2.0f);
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

    if (CollisionManager::GetInstance()->Raycast(aimRay, hit)) {
        aimPoint = hit.position;
    }

    return aimPoint;
}

std::unique_ptr<PlayerBullet> Player::CreateBullet(float& shotSpeed)
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

void Player::UpdateWeaponSwitch(Input* input)
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

void Player::SetHomingTargets(const std::vector<BaseEnemy*>& targets)
{
    *homingTargets_ = targets;
    std::erase_if(
        lockedHomingTargets_,
        [&targets](BaseEnemy* target) {
            return std::find(targets.begin(), targets.end(), target) == targets.end();
        });
}

void Player::UpdateHomingTarget(bool isLocking)
{
    if (!isLocking || currentWeapon_ != kWeaponHomingMissile ||
        camera_ == nullptr || lockedHomingTargets_.size() >= kMaxHomingLockCount) {
        return;
    }

    constexpr float kLockRadiusPixels = 120.0f;
    struct LockCandidate {
        BaseEnemy* enemy;
        float screenDistanceSquared;
    };
    std::vector<LockCandidate> candidates;
    for (BaseEnemy* enemy : *homingTargets_) {
        if (enemy == nullptr || enemy->IsDead()) {
            continue;
        }
        const Vector3 toEnemy = enemy->GetPosition() - transform_.translate;
        const float forwardDistance = Dot(toEnemy, railForward_);
        if (forwardDistance <= 0.0f ||
            forwardDistance > kHomingLockMaxForwardDistance) {
            continue;
        }
        const Vector2 screenPosition = camera_->WorldToScreen(enemy->GetPosition());
        const float differenceX = screenPosition.x - aimScreenPosition_.x;
        const float differenceY = screenPosition.y - aimScreenPosition_.y;
        const float distanceSquared = differenceX * differenceX + differenceY * differenceY;
        if (distanceSquared <= kLockRadiusPixels * kLockRadiusPixels) {
            candidates.push_back({ enemy, distanceSquared });
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const LockCandidate& left, const LockCandidate& right) {
            return left.screenDistanceSquared < right.screenDistanceSquared;
        });
    for (const LockCandidate& candidate : candidates) {
        if (std::find(
                lockedHomingTargets_.begin(),
                lockedHomingTargets_.end(),
                candidate.enemy) != lockedHomingTargets_.end()) {
            continue;
        }
        lockedHomingTargets_.push_back(candidate.enemy);
        if (lockedHomingTargets_.size() >= kMaxHomingLockCount) {
            break;
        }
    }
}

void Player::GetHomingLockPositions(std::vector<Vector3>& positions) const
{
    positions.clear();
    if (currentWeapon_ != kWeaponHomingMissile) {
        return;
    }
    for (BaseEnemy* target : lockedHomingTargets_) {
        if (target != nullptr && !target->IsDead()) {
            positions.push_back(target->GetPosition());
        }
    }
}

const char* Player::GetCurrentWeaponName() const
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

void Player::UpdateBullets()
{
    for (std::unique_ptr<PlayerBullet>& bullet : bullets_) {
        bullet->Update();
    }
}

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

    railOffset_ = ClampRailOffsetToScreen(nextRailOffset);
}

void Player::UpdateStarFoxMove()
{
    const float screenWidth =
        static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float screenHeight =
        static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
        return;
    }

    // The cursor behaves like an analog stick: the center is neutral and the
    // ship moves faster as the cursor gets farther from the center.
    float inputX = (aimScreenPosition_.x - screenWidth * 0.5f) /
        (screenWidth * 0.5f);
    float inputY = (screenHeight * 0.5f - aimScreenPosition_.y) /
        (screenHeight * 0.5f);

    constexpr float kDeadZone = 0.05f;
    constexpr float kStarFoxResponse = 2.75f;
    auto applyDeadZone = [](float value) {
        const float magnitude = std::abs(value);
        if (magnitude <= kDeadZone) {
            return 0.0f;
        }
        const float scaled = (magnitude - kDeadZone) / (1.0f - kDeadZone);
        return std::copysign(scaled, value);
    };

    inputX = applyDeadZone(std::clamp(
        inputX * mouseSensitivity_, -1.0f, 1.0f));
    inputY = applyDeadZone(std::clamp(
        inputY * mouseSensitivity_, -1.0f, 1.0f));

    constexpr float kSteeringLerpRate = 0.20f;
    starFoxSteeringInput_.x +=
        (inputX - starFoxSteeringInput_.x) * kSteeringLerpRate;
    starFoxSteeringInput_.y +=
        (inputY - starFoxSteeringInput_.y) * kSteeringLerpRate;

    Vector3 nextRailOffset = railOffset_;
    nextRailOffset.x +=
        starFoxSteeringInput_.x * moveSpeed_ * kStarFoxResponse;
    nextRailOffset.y +=
        starFoxSteeringInput_.y * moveSpeed_ * kStarFoxResponse;
    railOffset_ = ClampRailOffsetToScreen(nextRailOffset);
}

void Player::UpdateRolling(Input* input)
{
    if (isRolling_) {
        rollTimer_++;
        float progress = static_cast<float>(rollTimer_) / static_cast<float>(kRollDuration);
        transform_.rotate.z = rollDirection_ * progress * 2.0f * std::numbers::pi_v<float>;

        if (rollTimer_ >= kRollDuration) {
            isRolling_ = false;
            transform_.rotate.z = 0.0f;
            rollCooldown_ = kRollCooldownDuration;
        }
        return;
    }

    if (rollCooldown_ > 0) {
        rollCooldown_--;
    }

    if (leftKeyTapTimer_ > 0) {
        leftKeyTapTimer_--;
    }
    if (rightKeyTapTimer_ > 0) {
        rightKeyTapTimer_--;
    }

    if (rollCooldown_ <= 0) {
        if (input->IsKeyTrigger(DIK_A)) {
            if (leftKeyTapTimer_ > 0) {
                isRolling_ = true;
                rollTimer_ = 0;
                rollDirection_ = 1.0f;
                leftKeyTapTimer_ = 0;
                return;
            } else {
                leftKeyTapTimer_ = kMaxTapInterval;
            }
        }

        if (input->IsKeyTrigger(DIK_D)) {
            if (rightKeyTapTimer_ > 0) {
                isRolling_ = true;
                rollTimer_ = 0;
                rollDirection_ = -1.0f;
                rightKeyTapTimer_ = 0;
                return;
            } else {
                rightKeyTapTimer_ = kMaxTapInterval;
            }
        }
    }
}

Vector3 Player::CalculateRailWorldPosition(const Vector3& railOffset) const
{
    return railBasePosition_ + railRight_ * railOffset.x + railUp_ * railOffset.y + railForward_ * railOffset.z;
}

Vector3 Player::ClampRailOffsetToScreen(const Vector3& railOffset) const
{
    Vector3 correctedRailOffset = railOffset;
    correctedRailOffset.x = std::clamp(
        correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
    correctedRailOffset.y = std::clamp(
        correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
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

        correctedRailOffset.x = std::clamp(
            correctedRailOffset.x, -railMoveLimitX_, railMoveLimitX_);
        correctedRailOffset.y = std::clamp(
            correctedRailOffset.y, -railMoveLimitY_, railMoveLimitY_);
        correctedRailOffset.z = 0.0f;
    }

    return correctedRailOffset;
}

void Player::UpdateScreenBounds(const Vector3& worldPosition, float& minX, float& maxX, float& minY, float& maxY) const
{
    if (camera_ == nullptr) {
        return;
    }

    Vector2 screenPosition = camera_->WorldToScreen(worldPosition);
    minX = (std::min)(minX, screenPosition.x);
    maxX = (std::max)(maxX, screenPosition.x);
    minY = (std::min)(minY, screenPosition.y);
    maxY = (std::max)(maxY, screenPosition.y);
}

void Player::DrawImGui()
{
#ifdef _DEBUG
    ImGui::Begin("Player Controls");
    ImGui::Text("HP: %d / %d", currentHp_, maxHp_);
    ImGui::Text("Weapon: %s", GetCurrentWeaponName());
    ImGui::Text("Heat: %.2f", minigunHeat_);
    ImGui::End();
#endif
}
