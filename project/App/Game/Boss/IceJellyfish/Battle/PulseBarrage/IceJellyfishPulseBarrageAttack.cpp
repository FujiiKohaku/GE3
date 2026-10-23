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
    if (boss.player_ == nullptr || boss.bulletModel_ == nullptr || count <= 0) return;
    const Vector3 muzzle = boss.bodyPosition_ + Vector3 { 0.0f, -2.0f, -15.0f };
    const Vector3 targetDirection = Normalize(boss.player_->GetTranslate() - muzzle);
    int ringCount = count;

    if (includeCenter) {
        auto centerBullet = std::make_unique<NormalEnemyBullet>();
        centerBullet->Initialize(boss.bulletModel_);
        centerBullet->SetTranslate(muzzle);
        centerBullet->SetVelocity(targetDirection * speed);
        centerBullet->SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
        centerBullet->SetScale({ 0.46f, 0.46f, 0.95f });
        centerBullet->SetDamage(1);
        boss.AddEnemyBullet(std::move(centerBullet));
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
        bullet->Initialize(boss.bulletModel_);
        bullet->SetTranslate(muzzle);
        bullet->SetVelocity(direction * speed);
        bullet->SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
        bullet->SetScale({ 0.42f, 0.42f, 0.90f });
        bullet->SetDamage(1);
        boss.AddEnemyBullet(std::move(bullet));
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
        pulseBarrageTimer_ >= static_cast<float>(pulseBarrageWave_) * 0.18f) {
            int32_t aliveTentacles = 0;
            for (size_t tentacle = 0; tentacle < IceJellyfish::kTentacleCount; ++tentacle) {
                if (boss.IsSegmentAlive(tentacle, 0)) ++aliveTentacles;
            }

            if (pulseBarrageWave_ == 0) {
                FireRing(boss, aliveTentacles * 2, 0.12f, 0.65f, 0.0f, false);
            } else if (pulseBarrageWave_ == 1) {
                const int count = aliveTentacles * 2;
                float halfStep = 0.0f;
                if (count > 0) {
                    halfStep = std::numbers::pi_v<float> / static_cast<float>(count);
                }
                FireRing(boss, count, 0.22f, 0.62f, halfStep, false);
            } else {
                FireRing(boss, aliveTentacles, 0.08f, 0.82f,
                std::numbers::pi_v<float> * 0.5f, true);
            }
            ++pulseBarrageWave_;
        }
        if (pulseBarrageWave_ >= 3) pulseBarrageWave_ = -1;
    }

}
