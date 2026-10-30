#include "IceJellyfishPulseBarrageAttack.h"
#include "../IceJellyfishBattleState.h"
#include "../../IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include "Engine/Effect/EffectManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace {
// 調整用の設定値。意味が異なる値は、数値が同じでも個別に保持する。
constexpr Vector3 kMuzzleOffset = { 0.0f, -2.0f, -15.0f };
constexpr Vector4 kBulletColor = { 1.0f, 0.0f, 0.0f, 1.0f };
constexpr Vector3 kCenterBulletScale = { 0.46f, 0.46f, 0.95f };
constexpr Vector3 kRingBulletScale = { 0.42f, 0.42f, 0.90f };
constexpr int kBulletDamage = 1;
constexpr float kWaveIntervalSeconds = 0.18f;
constexpr float kFirstWaveSpread = 0.12f;
constexpr float kFirstWaveSpeed = 0.65f;
constexpr float kSecondWaveSpread = 0.22f;
constexpr float kSecondWaveSpeed = 0.62f;
constexpr float kFinalWaveSpread = 0.08f;
constexpr float kFinalWaveSpeed = 0.82f;

constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
}

void IceJellyfishPulseBarrageAttack::Reset()
{
    pulseBarrageWave_ = -1;
    pulseBarrageTimer_ = 0.0f;
}

void IceJellyfishPulseBarrageAttack::FireRing(IceJellyfish& boss,
    int count, float spread, float speed, float angleOffset, bool includeCenter)
{
    if (!boss.HasAttackTarget() || boss.GetAttackBulletModel() == nullptr || count <= 0) return;
    const Vector3 muzzle = boss.GetAttackBodyPosition() + kMuzzleOffset;
    const Vector3 targetDirection = Normalize(boss.GetAttackTargetPosition() - muzzle);
    int ringCount = count;

    if (includeCenter) {
        auto centerBullet = std::make_unique<NormalEnemyBullet>();
        centerBullet->Initialize(boss.GetAttackBulletModel());
        centerBullet->SetTranslate(muzzle);
        centerBullet->SetVelocity(targetDirection * speed);
        centerBullet->SetColor(kBulletColor);
        centerBullet->SetScale(kCenterBulletScale);
        centerBullet->SetDamage(kBulletDamage);
        boss.SpawnAttackBullet(std::move(centerBullet));
        --ringCount;
    }

    for (int32_t index = 0; index < ringCount; ++index) {
        const float angle = angleOffset + kTwoPi * static_cast<float>(index) /
            static_cast<float>(ringCount);
        const Vector3 direction = Normalize(targetDirection + Vector3 {
            std::cos(angle) * spread,
            std::sin(angle) * spread,
            0.0f });
        auto bullet = std::make_unique<NormalEnemyBullet>();
        bullet->Initialize(boss.GetAttackBulletModel());
        bullet->SetTranslate(muzzle);
        bullet->SetVelocity(direction * speed);
        bullet->SetColor(kBulletColor);
        bullet->SetScale(kRingBulletScale);
        bullet->SetDamage(kBulletDamage);
        boss.SpawnAttackBullet(std::move(bullet));
    }
}

void IceJellyfishPulseBarrageAttack::Start()
{
    pulseBarrageWave_ = 0;
    pulseBarrageTimer_ = 0.0f;
}

void IceJellyfishPulseBarrageAttack::Update(IceJellyfish& boss, float deltaTimeSeconds)
{
    if (pulseBarrageWave_ >= 0) {
        pulseBarrageTimer_ += deltaTimeSeconds;
        while (pulseBarrageWave_ < 3 &&
        pulseBarrageTimer_ >= static_cast<float>(pulseBarrageWave_) * kWaveIntervalSeconds) {
            int32_t aliveTentacles = 0;
            for (size_t tentacle = 0; tentacle < IceJellyfish::kTentacleCount; ++tentacle) {
                if (boss.IsSegmentAlive(tentacle, 0)) ++aliveTentacles;
            }

            if (pulseBarrageWave_ == 0) {
                FireRing(boss, aliveTentacles * 2, kFirstWaveSpread, kFirstWaveSpeed, 0.0f, false);
            } else if (pulseBarrageWave_ == 1) {
                const int count = aliveTentacles * 2;
                float halfStep = 0.0f;
                if (count > 0) {
                    halfStep = std::numbers::pi_v<float> / static_cast<float>(count);
                }
                FireRing(boss, count, kSecondWaveSpread, kSecondWaveSpeed, halfStep, false);
            } else {
                FireRing(boss, aliveTentacles, kFinalWaveSpread, kFinalWaveSpeed,
                std::numbers::pi_v<float> * 0.5f, true);
            }
            ++pulseBarrageWave_;
        }
        if (pulseBarrageWave_ >= 3) pulseBarrageWave_ = -1;
    }

}
