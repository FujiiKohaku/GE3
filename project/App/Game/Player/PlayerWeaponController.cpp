#include "App/Game/Player/Player.h"
#include "Engine/Effect/EffectManager.h"
#include <cmath>
#include "App/Game/Common/CombatTarget.h"
#include "App/Game/Common/GameTiming.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Enemy/BaseEnemy.h"
#include <algorithm>
#include "Engine/audio/SoundManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/Input/Input.h"
#include "Engine/CollisionManager/CollisionManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Winapp/WinApp.h"
#include <numbers>
#include "Engine/math/MatrixMath.h"
#include "Engine/3D/Object3dManager.h"
#include <cassert>

// PlayerWeaponController
// 選択中の武器を切り替え、発射・発熱・ロックオンと生成済みのプレイヤー弾を管理する。

// クールダウンの残り時間を減らし、0未満になった場合は0に揃える。
void PlayerWeaponController::UpdateCooldowns(float deltaTimeSeconds)
{
    missileCooldownRemainingSeconds_ -= deltaTimeSeconds;
    if (missileCooldownRemainingSeconds_ < 0.0) {
        missileCooldownRemainingSeconds_ = 0.0;
    }
    homingCooldownRemainingSeconds_ -= deltaTimeSeconds;
    if (homingCooldownRemainingSeconds_ < 0.0) {
        homingCooldownRemainingSeconds_ = 0.0;
    }
}

// 選択中の武器が発射可能かどうかを返す。クールダウン中はfalse。
bool PlayerWeaponController::IsWeaponReady() const
{
    constexpr float kCooldownToleranceSeconds = 0.000001f;
    if (currentWeapon_ == kWeaponMissileBullet) {
        if (missileCooldownRemainingSeconds_ > kCooldownToleranceSeconds) {
            return false;
        }
        return true;
    }
    if (currentWeapon_ == kWeaponHomingMissile) {
        if (homingCooldownRemainingSeconds_ > kCooldownToleranceSeconds) {
            return false;
        }
        return true;
    }
    return true;
}

// 発射後に、選択中の武器のクールダウンを開始する。
void PlayerWeaponController::StartCooldown()
{
    if (currentWeapon_ == kWeaponMissileBullet) { missileCooldownRemainingSeconds_ = kMissileFireIntervalSeconds; }
    if (currentWeapon_ == kWeaponHomingMissile) { homingCooldownRemainingSeconds_ = kHomingFireIntervalSeconds; }
}

float PlayerWeaponController::GetMissileReloadRatio() const
{
    if (IsWeaponReady()) { return 1.0f; }       
    if (currentWeapon_ == kWeaponMissileBullet) {
        return std::clamp(static_cast<float>(1.0 - missileCooldownRemainingSeconds_ / kMissileFireIntervalSeconds), 0.0f, 1.0f);
    }
    return std::clamp(static_cast<float>(1.0 - homingCooldownRemainingSeconds_ / kHomingFireIntervalSeconds), 0.0f, 1.0f);
}

int PlayerWeaponController::GetRequestedShotCount(bool isHeld, bool isTriggered, float deltaTimeSeconds)
{
    if (currentWeapon_ != kWeaponAssault) {
        if (isTriggered) { return 1; }
        return 0;
    }
    int requestedShotCount = 0;
    if (isTriggered) {
        assaultCooldownSeconds_ = 0.0f;
        ++requestedShotCount;
    }
    if (isHeld) {
        assaultCooldownSeconds_ += deltaTimeSeconds;
        while (assaultCooldownSeconds_ + 0.000001f >= kAssaultFireIntervalSeconds) {
            assaultCooldownSeconds_ = (std::max)(0.0f, assaultCooldownSeconds_ - kAssaultFireIntervalSeconds);
            ++requestedShotCount;
        }
    }
    return requestedShotCount;
}
// 機体の姿勢と状態を参照する。参照先のPlayerは、この管理クラスより長く生存する必要がある。
PlayerWeaponController::PlayerWeaponController(const Player& player)
    : transform_(player.transform_), player_(player)
{
}

// 前回の戦闘状態を破棄し、プレイヤー弾の表示に使うモデルを読み込む。
void PlayerWeaponController::Initialize()
{
    ResetCombatState();
    bulletModel_ = ModelManager::GetInstance()->Load("Enemy/Bullet/LowPolySphere/lowPolySphere.obj");
}

void PlayerWeaponController::ResetCombatState()
{
    // ミサイルの航跡など、弾種固有の終了処理を実行してから破棄する。
    for (const auto& bullet : bullets_) {
        bullet->SetDead();
    }
    bullets_.clear();
    ClearHomingTargets();
    heatRatio_ = 0.0f;
    isOverheated_ = false;
    graceRemainingSeconds_ = 0.0f;
    shotsFired_ = 0;
    isFireBlocked_ = false;
    wasFireHeld_ = false;
    lastFocusLossCount_ = 0;
    currentWeapon_ = kWeaponAssault;
    assaultCooldownSeconds_ = 0.0f;
    missileCooldownRemainingSeconds_ = 0.0;
    homingCooldownRemainingSeconds_ = 0.0;
#ifdef _DEBUG
    shouldDrawDebugLines_ = false;
    debugAimRayOrigin_ = {};
    debugAimPoint_ = {};
    debugMuzzlePosition_ = {};
    debugDrawRayOrigin_ = {};
    debugDrawAimPoint_ = {};
#endif
}

// 新しいカメラを記録し、すでに発射済みの弾にも同じカメラを設定する。
void PlayerWeaponController::SetCamera(Camera* camera)
{
    camera_ = camera;
    for (const auto& bullet : bullets_) {
        bullet->SetCamera(camera);
    }
}

void PlayerWeaponController::BlockFireUntilRelease()
{
    isFireBlocked_ = true;
    wasFireHeld_ = false;
    ResetHomingLock();
}

bool PlayerWeaponController::UpdateFireInputState(bool isFireHeld, bool isForeground, uint64_t focusLossCount)
{
    if (focusLossCount != lastFocusLossCount_) {
        lastFocusLossCount_ = focusLossCount;
        BlockFireUntilRelease();
    }
    if (!isForeground) {
        BlockFireUntilRelease();
        return false;
    }
    if (isFireBlocked_) {
        if (!isFireHeld) {
            isFireBlocked_ = false;
        }
        return false;
    }
    const bool isTriggered = isFireHeld && !wasFireHeld_;
    wasFireHeld_ = isFireHeld;
    return isTriggered;
}
// 武器切替・冷却・ミサイル間隔を更新し、選択中の武器へ射撃入力を渡す。
void PlayerWeaponController::UpdateInput(Input* input, bool isDebugMode)
{
    if (!isDebugMode) UpdateWeaponSwitch(input);
    const float dt = TimeManager::GetInstance()->GetDeltaTime();
    UpdateCooldowns(dt);
    const bool isHoming = IsHomingMissileSelected();
    const bool isAssault = currentWeapon_ == kWeaponAssault;
    if (isDebugMode || !isHoming) ResetHomingLock();
    const bool canFire = !player_.IsDead() && bulletModel_ != nullptr && camera_ != nullptr && !isDebugMode;
    const bool isPhysicalFireHeld = input->IsKeyPressed(DIK_SPACE) || input->IsMousePressed(0) ||
        input->IsGamepadButtonPressed(XINPUT_GAMEPAD_A) || input->IsGamepadRightTriggerPressed();
    bool isTriggered = UpdateFireInputState(isPhysicalFireHeld, input->IsForeground(), input->GetFocusLossCount());
    const bool isFireHeld = isPhysicalFireHeld && !isFireBlocked_ && input->IsForeground();
    UpdateHeat(dt, canFire && isAssault && isFireHeld);
    if (isHoming) {
        isTriggered = UpdateHomingFireInput(isFireHeld, canFire && IsWeaponReady(),
            camera_, transform_.translate, player_.GetFlightForward(), player_.GetAimScreenPosition());
    }
    if (!canFire) { return; }
    const int requestedShotCount = GetRequestedShotCount(isFireHeld, isTriggered, dt);
    for (int shotIndex = 0; shotIndex < requestedShotCount; ++shotIndex) {
        const bool isFired = FireBullet(*camera_);
        if (isFired && isHoming) { lockedTargets_.clear(); }
    }
}
// 追尾ミサイル選択中だけ、ロック表示に必要な敵の位置を書き出す。
void PlayerWeaponController::GetHomingLockPositions(std::vector<Vector3>& positions) const
{
    positions.clear();
    if (IsHomingMissileSelected()) {
        GetLockedPositions(positions);
    }
}

void PlayerWeaponController::DrawBullets()
{
    for (const auto& bullet : bullets_) {
        bullet->Draw();
    }
}

// 描画対象をレイトレーシング用のシーンへ登録する。
void PlayerWeaponController::SubmitRaytracing(DxrRenderer& renderer) {
    for (const auto& bullet : bullets_) {
        if (bullet) { bullet->SubmitRaytracing(renderer); }
    }
}

// 現在の武器へ発射を要求し、その武器固有の過熱やクールダウン判定を使う。
bool PlayerWeaponController::FireBullet(const Camera& activeCamera)
{
    if (player_.IsDead() || bulletModel_ == nullptr || camera_ == nullptr) { return false; }
    const bool isAssault = currentWeapon_ == kWeaponAssault;
    const bool isHoming = IsHomingMissileSelected();
    if (isAssault && IsOverheated()) { return false; }
    if (!IsWeaponReady()) { return false; }
    if (isAssault) {
        SoundManager::GetInstance()->Play("AssaultShot");
    } else {
        SoundManager::GetInstance()->Play("MissileShot");
    }
    if (isHoming && !lockedTargets_.empty()) {
        for (CombatTarget* target : lockedTargets_) {
            FireSingleBullet(activeCamera, target);
        }
    } else {
        FireSingleBullet(activeCamera, nullptr);
    }
    StartCooldown();
    if (isAssault) { AddShotHeat(); }
    return true;
}
// 武器に弾を生成させ、熱量の色・銃口位置・照準方向・機体速度を反映して所有する。
void PlayerWeaponController::FireSingleBullet(const Camera& activeCamera, CombatTarget* homingTarget)
{

    float shotSpeed = bulletSpeed_;
    std::unique_ptr<PlayerBullet> bullet;
    if (currentWeapon_ == kWeaponAssault) {
        auto assaultBullet = std::make_unique<NormalBullet>();
        assaultBullet->Initialize(bulletModel_);
        assaultBullet->SetDamage(GetShotDamage(kAssaultBaseDamage));
        bullet = std::move(assaultBullet);
    } else if (currentWeapon_ == kWeaponMissileBullet) {
        auto missileBullet = std::make_unique<MissileBullet>();
        shotSpeed = missileBullet->GetSpeed() / 60.0f;
        missileBullet->Initialize(bulletModel_);
        bullet = std::move(missileBullet);
    } else {
        auto homingBullet = std::make_unique<HomingMissileBullet>();
        shotSpeed = homingBullet->GetSpeed() / 60.0f;
        homingBullet->Initialize(bulletModel_);
        homingBullet->SetTarget(homingTarget, homingTargets_);
        bullet = std::move(homingBullet);
    }
    constexpr Vector4 kColdBulletColor = { 0.12f, 0.48f, 1.0f, 1.0f };
    constexpr Vector4 kHotBulletColor = { 0.85f, 0.015f, 0.008f, 1.0f };
    const float heatRatio = std::clamp(GetHeatRatio(), 0.0f, 1.0f);
    const Vector4 bulletColor = {
        kColdBulletColor.x + (kHotBulletColor.x - kColdBulletColor.x) * heatRatio,
        kColdBulletColor.y + (kHotBulletColor.y - kColdBulletColor.y) * heatRatio,
        kColdBulletColor.z + (kHotBulletColor.z - kColdBulletColor.z) * heatRatio,
        1.0f
    };
    bullet->SetColor(bulletColor);
    bullet->SetCamera(camera_);
    Vector3 muzzlePosition = CalculateMuzzlePosition();
    EffectManager::GetInstance()->PlayEffect("ShotBullet", muzzlePosition);

    bullet->SetTranslate(muzzlePosition);

    // 照準更新時に求めた目標点へ銃口を向ける。
    Vector3 aimPoint = player_.GetAimPoint();


#ifdef _DEBUG
    shouldDrawDebugLines_ = true;
    debugAimRayOrigin_ = player_.GetAimRayOrigin();
    debugAimPoint_ = aimPoint;
    debugMuzzlePosition_ = muzzlePosition;

    debugDrawRayOrigin_ = player_.GetAimRayOrigin();
    debugDrawAimPoint_ = player_.GetAimPoint();
#endif

    Vector3 bulletDirection = Normalize(aimPoint - muzzlePosition);
    Vector3 worldPlayerVelocity = player_.GetWorldVelocity();

    Vector3 bulletVelocity;
    bulletVelocity.x = bulletDirection.x * shotSpeed + worldPlayerVelocity.x;
    bulletVelocity.y = bulletDirection.y * shotSpeed + worldPlayerVelocity.y;
    bulletVelocity.z = bulletDirection.z * shotSpeed + worldPlayerVelocity.z;

    bullet->SetVelocity(bulletVelocity);

    // 発射後の弾は管理側が所有し、以後の更新・衝突判定・削除の対象にする。
    bullets_.push_back(std::move(bullet));
    ++shotsFired_;
}

// 機体に対する銃口の相対位置を、現在の姿勢でワールド位置へ変換する。
Vector3 PlayerWeaponController::CalculateMuzzlePosition() const
{
    Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Vector3 localMuzzle = { 0.0f, bulletSpawnOffsetY_, bulletSpawnOffsetZ_ };
    return MatrixMath::Transform(localMuzzle, worldMatrix);
}

// ホイール・方向キー・数字キーの入力を武器の選択番号へ反映する。
void PlayerWeaponController::UpdateWeaponSwitch(Input* input)
{
    if (input->GetMouseWheel() > 0 || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_RIGHT)) {
        currentWeapon_ = (currentWeapon_ + 1) % kWeaponCount;
    }

    if (input->GetMouseWheel() < 0 || input->IsGamepadButtonTrigger(XINPUT_GAMEPAD_DPAD_LEFT)) {
        currentWeapon_ = (currentWeapon_ + kWeaponCount - 1) % kWeaponCount;
    }

    if (input->IsKeyTrigger(DIK_1)) currentWeapon_ = kWeaponAssault;
    if (input->IsKeyTrigger(DIK_2)) currentWeapon_ = kWeaponMissileBullet;
    if (input->IsKeyTrigger(DIK_3)) currentWeapon_ = kWeaponHomingMissile;
}

const char* PlayerWeaponController::GetCurrentWeaponName() const
{
    if (currentWeapon_ == kWeaponMissileBullet) { return "ミサイル"; }
    if (currentWeapon_ == kWeaponHomingMissile) { return "ホーミングミサイル"; }
    return "アサルト";
}
// 所有しているプレイヤー弾を一フレームずつ更新する。
void PlayerWeaponController::UpdateBullets()
{
    for (std::unique_ptr<PlayerBullet>& bullet : bullets_) {
        bullet->Update();
    }
}

// 命中や寿命で消滅した弾を一覧から外し、所有していたオブジェクトを破棄する。
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
// 照準レイと銃口からの射線を線で表示し、狙いのずれを確認できるようにする。
void PlayerWeaponController::DrawAimDebugLines()
{
    if (shouldDrawDebugLines_) {
        // AimCameraから飛ぶRay (青)
        DebugRenderer::GetInstance()->AddLine(debugAimRayOrigin_, debugAimPoint_, { 0.0f, 0.0f, 1.0f, 1.0f }, 3.0f);

        // Playerのマズルから飛ぶ実際の弾の進行方向 (赤)
        DebugRenderer::GetInstance()->AddLine(debugMuzzlePosition_, debugAimPoint_, { 1.0f, 0.0f, 0.0f, 1.0f }, 3.0f);

        // 描画用Cameraから飛ぶRay (黄色 - デバッグ比較専用)
        DebugRenderer::GetInstance()->AddLine(debugDrawRayOrigin_, debugDrawAimPoint_, { 1.0f, 1.0f, 0.0f, 1.0f }, 3.0f);
    }
}
#endif

// 発熱と冷却の共通処理。
// 熱量が高いほど威力を増やす。発射時点の熱量でその弾の威力を決める。
int PlayerWeaponController::GetShotDamage(int baseDamage) const
{
    // 発射時の熱量で威力を確定する。0%で等倍、100%で2倍。
    if (baseDamage <= 0) { return 0; }
    return static_cast<int>(std::lround(baseDamage * (1.0f + heatRatio_)));
}

// 連射中の過熱猶予と、射撃停止中・過熱中の冷却を進める。
void PlayerWeaponController::UpdateHeat(float deltaTimeSeconds, bool isFireHeld)
{
    if (deltaTimeSeconds <= 0.0f) {
        return;
    }
    bool shouldCoolHeat = !isFireHeld || isOverheated_;
    if (graceRemainingSeconds_ > 0.0f) {
        if (isFireHeld) {
            shouldCoolHeat = false;
            graceRemainingSeconds_ = (std::max)(0.0f,
                graceRemainingSeconds_ - deltaTimeSeconds);
            if (graceRemainingSeconds_ <= 0.0f) {
                isOverheated_ = true;
            }
        } else {
            graceRemainingSeconds_ = 0.0f;
        }
    }
    float coolingPerSecond = kCoolingPerSecond;
    if (isOverheated_) {
        coolingPerSecond = kOverheatCoolingPerSecond;
    }
    if (shouldCoolHeat) {
        heatRatio_ = (std::max)(0.0f,
            heatRatio_ - coolingPerSecond * deltaTimeSeconds);
    }
    if (isOverheated_ && heatRatio_ <= 0.0f) {
        isOverheated_ = false;
    }
}

// アサルト一発分の熱量を加え、満タンになったときの猶予を準備する。
void PlayerWeaponController::AddShotHeat()
{
    if (isOverheated_) {
        return;
    }
    heatRatio_ = (std::min)(1.0f, heatRatio_ + kAssaultShotHeat);
    if (heatRatio_ >= 1.0f && graceRemainingSeconds_ <= 0.0f) {
        graceRemainingSeconds_ = kGraceDurationSeconds;
    }
}

// ロック対象の管理と発射タイミング。
namespace {
struct LockCandidate {
    CombatTarget* enemy;
    float screenDistanceSquared;
};

bool CompareLockCandidates(const LockCandidate& left, const LockCandidate& right)
{
    return left.screenDistanceSquared < right.screenDistanceSquared;
}
}

// 現在の候補一覧を共有データへ反映し、弾が対象の生存を確認できるようにする。
void PlayerWeaponController::SetHomingTargets(const std::vector<CombatTarget*>& targets)
{
    *homingTargets_ = targets;
    for (auto targetIterator = lockedTargets_.begin(); targetIterator != lockedTargets_.end();) {
        if (std::find(targets.begin(), targets.end(), *targetIterator) == targets.end()) {
            targetIterator = lockedTargets_.erase(targetIterator);
        } else {
            ++targetIterator;
        }
    }
}

void PlayerWeaponController::ClearHomingTargets()
{
    homingTargets_->clear();
    ResetHomingLock();
}

// ロック対象と前回の押下状態を消し、次の射撃入力を新しく受け付ける。
void PlayerWeaponController::ResetHomingLock()
{
    lockedTargets_.clear();
    wasHomingFireHeld_ = false;
}

// 押下中のロック更新と、ボタンを離した瞬間の発射要求を判定する。
bool PlayerWeaponController::UpdateHomingFireInput(bool isFireHeld, bool isEnabled, Camera* camera,
    const Vector3& position, const Vector3& forward, const Vector2& aimPosition)
{
    if (!isEnabled) {
        ResetHomingLock();
        return false;
    }
    if (isFireHeld && !wasHomingFireHeld_) {
        lockedTargets_.clear();
    }
    UpdateHomingTargets(isFireHeld, camera, position, forward, aimPosition);
    const bool shouldFire = !isFireHeld && wasHomingFireHeld_ && camera != nullptr;
    wasHomingFireHeld_ = isFireHeld;
    return shouldFire;
}

// 前方距離と画面上の照準との近さを調べ、条件を満たす敵を上限数までロックする。
void PlayerWeaponController::UpdateHomingTargets(bool isLocking, Camera* camera,
    const Vector3& position, const Vector3& forward, const Vector2& aimPosition)
{
    if (!isLocking ||
        camera == nullptr || lockedTargets_.size() >= kMaxLockCount) {
        return;
    }

    const std::size_t previousLockCount = lockedTargets_.size();
    constexpr float kLockRadiusPixels = 120.0f;
    std::vector<LockCandidate> candidates;
    for (CombatTarget* enemy : *homingTargets_) {
        if (enemy == nullptr || enemy->IsDead()) {
            continue;
        }
        const Vector3 toEnemy = enemy->GetPosition() - position;
        const float forwardDistance = Dot(toEnemy, forward);
        if (forwardDistance <= 0.0f ||
            forwardDistance > kMaxForwardDistance) {
            continue;
        }
        const Vector2 screenPosition = camera->WorldToScreen(enemy->GetPosition());
        const float differenceX = screenPosition.x - aimPosition.x;
        const float differenceY = screenPosition.y - aimPosition.y;
        const float distanceSquared = differenceX * differenceX + differenceY * differenceY;
        if (distanceSquared <= kLockRadiusPixels * kLockRadiusPixels) {
            candidates.push_back({ enemy, distanceSquared });
        }
    }

    std::sort(candidates.begin(), candidates.end(), CompareLockCandidates);
    for (const LockCandidate& candidate : candidates) {
        if (std::find(
                lockedTargets_.begin(),
                lockedTargets_.end(),
                candidate.enemy) != lockedTargets_.end()) {
            continue;
        }
        lockedTargets_.push_back(candidate.enemy);
        if (lockedTargets_.size() >= kMaxLockCount) {
            break;
        }
    }
    if (lockedTargets_.size() > previousLockCount) {
        SoundManager::GetInstance()->Play("HomingLock");
    }
}

// 生存中のロック対象の位置を集め、画面のロックマーカー表示に使う。
void PlayerWeaponController::GetLockedPositions(std::vector<Vector3>& positions) const
{
    positions.clear();
    for (CombatTarget* target : lockedTargets_) {
        if (target != nullptr && !target->IsDead()) {
            positions.push_back(target->GetPosition());
        }
    }
}

