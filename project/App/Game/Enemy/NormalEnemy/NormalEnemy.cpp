// 通常敵の射撃と難易度設定を扱う。発射間隔・弾速・HPなどを設定から変更できる。
#include "App/Game/Common/GameTiming.h"
#include "Engine/Time/TimeManager.h"
#include "App/Game/Enemy/NormalEnemy/NormalEnemy.h"

#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"

#include "App/Game/Player/Player.h"
#include "Engine/math/MathStruct.h"
#include "Engine/Logger/Logger.h"
#include <cmath>

// 通常敵の初期HP・弾速・発射間隔と、射撃に使う参照を設定する。
void NormalEnemy::Initialize(
    Model* model,
    Model* bulletModel,
    Player* player)
{
    BaseEnemy::Initialize(model);
    SetHealth(4.0f);

    bulletModel_ = bulletModel;

    player_ = player;
    fireIntervalSeconds_ = 2.0f;
    fireElapsedSeconds_ = 0.0f;
    bulletSpeed_ = 0.48f;
    isRadialShotEnabled_ = true;
}

// 基底クラスの移動・攻撃・外見更新を使って、通常敵を一フレーム進める。
void NormalEnemy::Update()
{
    BaseEnemy::Update();
}

// 設定値を検証してからHP・弾速・発射間隔へ反映し、不正な設定は従来値へ戻す。
void NormalEnemy::ApplySettings(const NormalEnemySettings& settings)
{
    NormalEnemySettings appliedSettings = settings;
    if (!IsValidNormalEnemySettings(appliedSettings)) {
        Logger::Warning("Invalid normal enemy settings; using legacy settings");
        appliedSettings = {};
    }
    SetHealth(appliedSettings.hp);
    SetFireInterval(appliedSettings.fireIntervalFrames);
    SetBulletSpeed(appliedSettings.bulletSpeedPerFrame);
    SetRadialShotEnabled(appliedSettings.isRadialShotEnabled);
    fireElapsedSeconds_ = 0.0f;
}

void NormalEnemy::SetRadialShotEnabled(bool isEnabled)
{
    isRadialShotEnabled_ = isEnabled;
}

void NormalEnemy::SetBulletSpeed(float bulletSpeed)
{
    bulletSpeed_ = bulletSpeed;
}

void NormalEnemy::SetFireInterval(int32_t fireIntervalFrames)
{
    fireIntervalSeconds_ = static_cast<float>(fireIntervalFrames) / 60.0f;
}

// プレイヤーの前方かつ射程内にいる場合だけ発射タイマーを進める。
void NormalEnemy::Attack()
{
    if (player_ == nullptr) {
        return;
    }

    Vector3 playerPosition = player_->GetTranslate();
    Vector3 toEnemy = transform_.translate - playerPosition;

    Vector3 playerVelocity = player_->GetAutomaticWorldVelocity();
    Vector3 forward = { 0.0f, 0.0f, 1.0f };
    float speedLength = Vector3Length(playerVelocity);
    if (speedLength > 0.001f) {
        forward = Normalize(playerVelocity);
    }

    float dot = toEnemy.x * forward.x + toEnemy.y * forward.y + toEnemy.z * forward.z;

    if (dot <= 0.0f) {
        return;
    }

    float distance = Vector3Length(toEnemy);

    if (distance <= 170.0f) {
        const int32_t shotCount = GameTiming::AdvanceInterval(fireElapsedSeconds_,
            fireIntervalSeconds_, TimeManager::GetInstance()->GetDeltaTime(), 3);
        for (int32_t shotIndex = 0; shotIndex < shotCount; shotIndex++) {
            FireBullet();
        }
    }
}

// プレイヤーへ向かう弾と、設定に応じた放射状の追加弾を発射する。
void NormalEnemy::FireBullet()
{
    if (player_ == nullptr) {
        return;
    }

    if (bulletModel_ == nullptr) {
        return;
    }

    Vector3 playerPosition = player_->GetTranslate();

    // 弾の到達時間からPlayerの自動前進位置を予測する。
    // 65%だけ先読みし、上下左右へ動けば避けられる余地は残す。
    constexpr float kPredictionRatio = 0.65f;
    const float distance = Vector3Length(
        playerPosition - transform_.translate);
    if (bulletSpeed_ > 0.0f) {
        const float flightFrames = distance / bulletSpeed_;
        playerPosition += player_->GetAutomaticWorldVelocity() *
            (flightFrames * kPredictionRatio);
    }

    constexpr float kMinimumAimDistance = 0.0001f;
    const Vector3 aimOffset = playerPosition - transform_.translate;
    const float aimDistance = Vector3Length(aimOffset);
    if (!std::isfinite(aimDistance) || aimDistance <= kMinimumAimDistance) {
        return;
    }

    const Vector3 aimDirection = Normalize(aimOffset);
    SpawnBullet(aimDirection, true);
    if (!isRadialShotEnabled_) {
        return;
    }

    constexpr int32_t kRadialBulletCount = 8;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kSpreadAngleDegrees = 15.0f;
    constexpr float kParallelAxisThreshold = 0.99f;
    const float spreadAngleRadians = kSpreadAngleDegrees * kPi / 180.0f;
    const float spreadCos = std::cos(spreadAngleRadians);
    const float spreadSin = std::sin(spreadAngleRadians);

    Vector3 referenceAxis = { 0.0f, 1.0f, 0.0f };
    if (std::abs(Dot(aimDirection, referenceAxis)) >= kParallelAxisThreshold) {
        referenceAxis = { 1.0f, 0.0f, 0.0f };
    }
    const Vector3 rightDirection = Normalize(Cross(referenceAxis, aimDirection));
    const Vector3 upDirection = Normalize(Cross(aimDirection, rightDirection));

    for (int32_t bulletIndex = 0; bulletIndex < kRadialBulletCount; ++bulletIndex) {
        const float angleRadians = 2.0f * kPi *
            static_cast<float>(bulletIndex) / static_cast<float>(kRadialBulletCount);
        const Vector3 radialDirection = rightDirection * std::cos(angleRadians) +
            upDirection * std::sin(angleRadians);
        const Vector3 direction = Normalize(
            aimDirection * spreadCos + radialDirection * spreadSin);
        SpawnBullet(direction, false);
    }
}

// 指定方向へ飛ぶ敵弾を生成し、弾管理へ引き渡す。
void NormalEnemy::SpawnBullet(const Vector3& direction, bool shouldPlayShotSound)
{
    std::unique_ptr<EnemyBullet> bullet = std::make_unique<NormalEnemyBullet>();
    bullet->Initialize(bulletModel_);
    bullet->SetTranslate(transform_.translate);
    bullet->SetVelocity(direction * bulletSpeed_);
    AddEnemyBullet(std::move(bullet), shouldPlayShotSound);
}
