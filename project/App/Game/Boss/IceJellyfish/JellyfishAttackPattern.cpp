#include "JellyfishAttackPattern.h"
#include "IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include <array>
#include <cmath>
#include <numbers>

namespace {
bool Crossed(float previous, float current, float point)
{
    return previous < point && current >= point;
}
}

void JellyfishAttackPattern::Enter(IceJellyfish& boss) const
{
    boss.attackTimer_ = 0.0f;
    boss.previousAttackTimer_ = 0.0f;
    boss.firedWave_ = 0;
    boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
    boss.absoluteInterrupted_ = false;
    boss.attackHitApplied_.fill(false);
    boss.absoluteSealDamage_.fill(0.0f);
    boss.crystalHp_.fill(0.0f);
    if (boss.player_) boss.lockedTarget_ = boss.player_->GetTranslate();
}
class FrozenSweepPattern final : public JellyfishAttackPattern {
public:
    JellyfishPatternType GetType() const override { return JellyfishPatternType::FrozenSweep; }
    float GetDuration(int phase) const override { return phase == 3 ? 5.4f : 6.0f; }
    void Update(IceJellyfish& boss) const override
    {
        if (Crossed(boss.previousAttackTimer_, boss.attackTimer_, 0.8f) && boss.player_ != nullptr) {
            boss.lockedTarget_ = boss.player_->GetTranslate();
        }
        if (Crossed(boss.previousAttackTimer_, boss.attackTimer_, 2.0f) && boss.player_ != nullptr &&
            std::abs(boss.player_->GetTranslate().y - boss.lockedTarget_.y) < 6.0f) {
            boss.DamagePlayerOnce(0, 2);
        }
        if (boss.phase_ == 3 && Crossed(boss.previousAttackTimer_, boss.attackTimer_, 3.0f) && boss.player_ != nullptr &&
            std::abs(boss.player_->GetTranslate().x - boss.lockedTarget_.x) < 6.0f) {
            boss.DamagePlayerOnce(1, 2);
        }
        boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
    }
};

class IcicleBloomPattern final : public JellyfishAttackPattern {
public:
    JellyfishPatternType GetType() const override { return JellyfishPatternType::IcicleBloom; }
    float GetDuration(int phase) const override { return 6.2f; }
    void Update(IceJellyfish& boss) const override
    {
        const int32_t waves = boss.phase_ == 1 ? 2 : 3;
        while (boss.firedWave_ < waves) {
            const float fireTime = 1.4f + static_cast<float>(boss.firedWave_) * 0.9f;
            if (!Crossed(boss.previousAttackTimer_, boss.attackTimer_, fireTime)) break;
            boss.FireRadialBurst(8, boss.phase_ == 3 ? 0.72f : 0.58f, 1,
                static_cast<float>(boss.firedWave_) * std::numbers::pi_v<float> / 8.0f);
            ++boss.firedWave_;
        }
        boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
    }
};

class TentacleThrustPattern final : public JellyfishAttackPattern {
public:
    JellyfishPatternType GetType() const override { return JellyfishPatternType::TentacleThrust; }
    float GetDuration(int phase) const override { return 6.5f; }
    void Update(IceJellyfish& boss) const override
    {
        if (Crossed(boss.previousAttackTimer_, boss.attackTimer_, 0.9f) && boss.player_ != nullptr) {
            boss.lockedTarget_ = boss.player_->GetTranslate();
        }
        const int32_t thrustCount = boss.phase_ == 1 ? 2 : 3;
        for (int32_t thrust = 0; thrust < thrustCount; ++thrust) {
            const float hitTime = 1.8f + static_cast<float>(thrust) * 0.85f;
            if (Crossed(boss.previousAttackTimer_, boss.attackTimer_, hitTime) && boss.player_ != nullptr) {
                const Vector3 difference = boss.player_->GetTranslate() - boss.lockedTarget_;
                if (Dot(difference, difference) <= 64.0f) boss.DamagePlayerOnce(thrust, 2);
                boss.lockedTarget_ = boss.player_->GetTranslate();
            }
        }
        boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
    }
};

class BlizzardCurrentPattern final : public JellyfishAttackPattern {
public:
    JellyfishPatternType GetType() const override { return JellyfishPatternType::BlizzardCurrent; }
    float GetDuration(int phase) const override { return 6.5f; }
    void Update(IceJellyfish& boss) const override
    {
        if (boss.player_ != nullptr && boss.attackTimer_ >= 1.0f && boss.attackTimer_ < 5.2f) {
            const float direction = (boss.patternIndex_ % 2 == 0) ? 1.0f : -1.0f;
            boss.player_->ApplyRailAreaForce({ direction * (boss.phase_ == 3 ? 0.13f : 0.09f), 0.0f, 0.0f });
        }
        while (boss.firedWave_ < 5) {
            const float fireTime = 1.3f + static_cast<float>(boss.firedWave_) * 0.75f;
            if (!Crossed(boss.previousAttackTimer_, boss.attackTimer_, fireTime)) break;
            boss.FireAimedBurst(3, 0.24f, 0.42f, 1);
            ++boss.firedWave_;
        }
        boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
    }
};

class CrystalPrisonPattern final : public JellyfishAttackPattern {
public:
    JellyfishPatternType GetType() const override { return JellyfishPatternType::CrystalPrison; }
    float GetDuration(int phase) const override { return 7.5f; }
    void Enter(IceJellyfish& boss) const override {
        JellyfishAttackPattern::Enter(boss);
        boss.crystalHp_.fill(6.0f);
    }
    void Exit(IceJellyfish& boss) const override { boss.crystalHp_.fill(0.0f); }
    void Update(IceJellyfish& boss) const override
    {
        boss.UpdateCrystalTransforms();
        int32_t alive = 0;
        for (float hp : boss.crystalHp_) {
            if (hp > 0.0f) ++alive;
        }
        if (Crossed(boss.previousAttackTimer_, boss.attackTimer_, 5.2f) && alive > 0 && boss.player_ != nullptr) {
            const Vector3 offset = boss.player_->GetTranslate() - boss.lockedTarget_;
            if (std::abs(offset.x) > 10.0f || std::abs(offset.y) > 10.0f) {
                boss.DamagePlayerOnce(0, 3);
            }
        }
        if (Crossed(boss.previousAttackTimer_, boss.attackTimer_, 5.6f) && alive > 0) {
            boss.FireAimedBurst(alive, 0.18f, 0.60f, 1);
        }
        boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
    }
};

class AbsoluteZeroPattern final : public JellyfishAttackPattern {
public:
    JellyfishPatternType GetType() const override { return JellyfishPatternType::AbsoluteZero; }
    float GetDuration(int phase) const override { return 8.0f; }
    void Update(IceJellyfish& boss) const override
    {
        int32_t released = 0;
        for (size_t tentacle = 0; tentacle < IceJellyfish::kTentacleCount; ++tentacle) {
            if (!boss.IsSegmentAlive(tentacle, IceJellyfish::kSegmentsPerTentacle - 1) ||
                boss.absoluteSealDamage_[tentacle] >= 3.0f) {
                ++released;
            }
        }
        boss.absoluteInterrupted_ = released == static_cast<int32_t>(IceJellyfish::kTentacleCount);
        boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
        if (!boss.absoluteInterrupted_ && Crossed(boss.previousAttackTimer_, boss.attackTimer_, 6.0f)) {
            boss.DamagePlayerOnce(0, 4);
            boss.FireRadialBurst(16, 0.78f, 2, 0.0f);
        }
    }
};

const JellyfishAttackPattern& GetJellyfishAttackPattern(JellyfishPatternType type)
{
    static const FrozenSweepPattern frozen;
    static const IcicleBloomPattern bloom;
    static const TentacleThrustPattern thrust;
    static const BlizzardCurrentPattern blizzard;
    static const CrystalPrisonPattern prison;
    static const AbsoluteZeroPattern absolute;
    static const std::array<const JellyfishAttackPattern*, 6> patterns {
        &frozen, &bloom, &thrust, &blizzard, &prison, &absolute };
    const auto index = static_cast<size_t>(type);
    return *patterns[index < patterns.size() ? index : 0];
}