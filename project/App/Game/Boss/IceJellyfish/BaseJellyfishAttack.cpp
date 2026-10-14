#include "BaseJellyfishAttack.h"
#include "IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace {
    constexpr float kIcePillarWaveTimeoutSeconds = 9.00f;
}

class PulseBarrageAttack final : public BaseJellyfishAttack {
public:
    void Start(IceJellyfish& boss) const override {
        boss.pulseBarrageWave_ = 0;
        boss.pulseBarrageTimer_ = 0.0f;
    }
    void Update(IceJellyfish& boss, float deltaTime) const override
    {
        if (boss.pulseBarrageWave_ >= 0) {
            boss.pulseBarrageTimer_ += deltaTime;
            while (boss.pulseBarrageWave_ < 3 &&
            boss.pulseBarrageTimer_ >= static_cast<float>(boss.pulseBarrageWave_) * 0.18f) {
                int32_t aliveTentacles = 0;
                for (size_t tentacle = 0; tentacle < IceJellyfish::kTentacleCount; ++tentacle) {
                    if (boss.IsSegmentAlive(tentacle, 0)) ++aliveTentacles;
                }

                if (boss.pulseBarrageWave_ == 0) {
                    boss.FirePulseRing(aliveTentacles * 2, 0.12f, 0.65f, 0.0f, false);
                } else if (boss.pulseBarrageWave_ == 1) {
                    const int count = aliveTentacles * 2;
                    const float halfStep = count > 0 ? std::numbers::pi_v<float> /
                    static_cast<float>(count) : 0.0f;
                    boss.FirePulseRing(count, 0.22f, 0.62f, halfStep, false);
                } else {
                    boss.FirePulseRing(aliveTentacles, 0.08f, 0.82f,
                    std::numbers::pi_v<float> * 0.5f, true);
                }
                ++boss.pulseBarrageWave_;
            }
            if (boss.pulseBarrageWave_ >= 3) boss.pulseBarrageWave_ = -1;
        }

    }
};

class IceSpearAttack final : public BaseJellyfishAttack {
public:
    void Start(IceJellyfish& boss) const override
    {
        boss.iceSpearTentacles_.fill(-1);
        std::array<int32_t, IceJellyfish::kTentacleCount> candidates {};
        size_t candidateCount = 0;
        for (size_t tentacle = 0; tentacle < IceJellyfish::kTentacleCount; ++tentacle) {
            if (boss.IsSegmentAlive(tentacle, 0)) {
                candidates[candidateCount++] = static_cast<int32_t>(tentacle);
            }
        }

        std::mt19937 randomEngine(std::random_device {}());
        std::shuffle(candidates.begin(), candidates.begin() + candidateCount, randomEngine);
        const size_t selectedCount = (std::min)(boss.iceSpearTentacles_.size(), candidateCount);
        for (size_t index = 0; index < selectedCount; ++index) {
            boss.iceSpearTentacles_[index] = candidates[index];
        }
        if (boss.player_ != nullptr) boss.lockedTarget_ = boss.player_->GetTranslate();
        boss.iceSpearVolley_ = selectedCount > 0 ? 0 : -1;
        boss.iceSpearTimer_ = 0.0f;
        boss.iceSpearPoseWeight_ = 0.0f;
    }
    void Update(IceJellyfish& boss, float deltaTime) const override
    {
        if (boss.iceSpearVolley_ >= 0) {
            boss.iceSpearTimer_ += deltaTime;
            if (boss.iceSpearVolley_ < 3) {
                boss.iceSpearPoseWeight_ = (std::min)(1.0f, boss.iceSpearPoseWeight_ + deltaTime * 2.8f);
                while (boss.iceSpearVolley_ < 3 &&
                boss.iceSpearTimer_ >= 1.0f + static_cast<float>(boss.iceSpearVolley_) * 0.20f) {
                    boss.FireIceSpearVolley();
                    ++boss.iceSpearVolley_;
                }
            }
            if (boss.iceSpearVolley_ >= 3) {
                boss.iceSpearPoseWeight_ = (std::max)(0.0f, boss.iceSpearPoseWeight_ - deltaTime * 1.8f);
                if (boss.iceSpearPoseWeight_ <= 0.0f) {
                    boss.iceSpearVolley_ = -1;
                    boss.iceSpearTentacles_.fill(-1);
                }
            }
        }

    }
};

class IcePillarAttack final : public BaseJellyfishAttack {
public:
    void Start(IceJellyfish& boss) const override
    {
        if (boss.player_ == nullptr || boss.icePillarWave_ >= 0) return;
        std::mt19937 randomEngine(std::random_device {}());
        std::uniform_int_distribution<int32_t> laneDistribution(0, 2);
        boss.icePillarSafeLane_ = laneDistribution(randomEngine);
        boss.icePillarWave_ = 0;
        boss.icePillarTimer_ = 0.0f;
        for (int32_t slot = 3; slot < 6; ++slot) boss.attackHitApplied_[slot] = false;
        boss.PrepareIcePillarWarning();
    }
    void Update(IceJellyfish& boss, float deltaTime) const override
    {
        if (boss.icePillarWave_ < 0) return;
        boss.icePillarTimer_ += deltaTime;
        if (boss.icePillarModelSpawnedForWave_) {
            boss.icePillarModelTimer_ += deltaTime;
        }
        if (!boss.icePillarModelSpawnedForWave_ &&
        boss.icePillarTimer_ >= kIcePillarRevealSeconds) {
            constexpr std::array<float, 3> spikeOffsets = { -4.0f, 0.0f, 4.0f };
            size_t modelIndex = 0;
            for (const Vector3& laneCenter : boss.icePillarPositions_) {
                for (size_t spike = 0; spike < spikeOffsets.size(); ++spike) {
                    boss.icePillarModelPositions_[modelIndex++] = {
                        laneCenter.x + spikeOffsets[spike],
                        laneCenter.y,
                        laneCenter.z + (spike == 1 ? 1.2f : -1.2f)
                    };
                }
            }
            boss.icePillarModelTimer_ = 0.0f;
            boss.icePillarModelSpawnedForWave_ = true;
        }

        if (boss.player_ != nullptr) {
            const Vector3 playerPosition = boss.player_->GetTranslate();
            if (boss.icePillarModelSpawnedForWave_) {
                for (const Vector3& position : boss.icePillarPositions_) {
                    const bool withinWidth = std::abs(playerPosition.x - position.x) <= 6.5f;
                    const bool withinDepth = std::abs(playerPosition.z - position.z) <= 4.0f;
                    const bool withinHeight = playerPosition.y >= position.y - 2.0f &&
                    playerPosition.y <= position.y + 40.0f;
                    if (withinWidth && withinDepth && withinHeight) {
                        boss.DamagePlayerOnce(3 + boss.icePillarWave_, 2);
                        break;
                    }
                }
            }

            const bool passedPillars = playerPosition.z > boss.icePillarPositions_[0].z + 10.0f;
            if (!passedPillars && boss.icePillarTimer_ < kIcePillarWaveTimeoutSeconds) return;
        } else if (boss.icePillarTimer_ < kIcePillarWaveTimeoutSeconds) {
            return;
        }

        boss.icePillarModelSpawnedForWave_ = false;
        boss.icePillarModelTimer_ = 0.0f;
        ++boss.icePillarWave_;
        boss.icePillarTimer_ = 0.0f;
        if (boss.icePillarWave_ >= 3) {
            boss.icePillarWave_ = -1;
            return;
        }
        boss.icePillarSafeLane_ = (boss.icePillarSafeLane_ + 1 + boss.icePillarWave_ % 2) % 3;
        boss.PrepareIcePillarWarning();
    }
};

const std::array<const BaseJellyfishAttack*, 3>& GetJellyfishAttacks()
{
    static const PulseBarrageAttack pulse;
    static const IceSpearAttack spear;
    static const IcePillarAttack pillar;
    static const std::array<const BaseJellyfishAttack*, 3> attacks { &pulse, &spear, &pillar };
    return attacks;
}
