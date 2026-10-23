#include "IceJellyfishIcePillarAttack.h"
#include "../../IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include "App/Game/Enemy/Bullet/Types/NormalEnemyBullet.h"
#include "Engine/Effect/EffectManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace {
constexpr float kIcePillarAheadDistance = 190.0f;
constexpr float kIcePillarRevealSeconds = 2.0f;
constexpr float kIcePillarWaveTimeoutSeconds = 9.0f;
}

Vector3 IceJellyfishIcePillarAttack::ApplyTentacleDirection(const IceJellyfish& boss,
    const IceJellyfishAttackMotionContext& context, const Vector3& direction) const
{
    if (!IsTentacleSelected(context.tentacleIndex)
        || !boss.IsSegmentAlive(context.tentacleIndex, context.segmentIndex)) {
        return direction;
    }
    const Vector3 floorDirection = Normalize(Vector3 {
        context.radialX * 0.08f, -1.0f, context.radialZ * 0.08f
    });
    const float pointDownWeight = GetCharge() * (0.35f + context.tipWeight * 0.25f);
    return Normalize(direction * (1.0f - pointDownWeight) + floorDirection * pointDownWeight);
}

Vector4 IceJellyfishIcePillarAttack::ApplyTentacleColor(const IceJellyfishAttackMotionContext& context,
    const Vector4& baseColor) const
{
    if (!IsTentacleSelected(context.tentacleIndex)) {
        return baseColor;
    }
    const float charge = GetCharge();
    return {
        0.55f + charge * 0.65f,
        0.80f + charge * 1.35f,
        1.00f + charge * 1.70f,
        baseColor.w
    };
}

void IceJellyfishIcePillarAttack::Reset()
{
    hasHitPlayerByWave_.fill(false);
    icePillarWave_ = -1;
    icePillarSafeLane_ = 1;
    icePillarTimer_ = 0.0f;
    icePillarPositions_ = {};
    icePillarModelPositions_ = {};
    icePillarModelTimer_ = 0.0f;
    icePillarModelSpawnedForWave_ = false;
}

void IceJellyfishIcePillarAttack::PrepareWarning(IceJellyfish& boss)
{
    if (boss.player_ == nullptr || icePillarWave_ < 0 || icePillarWave_ >= 3) return;
    icePillarModelSpawnedForWave_ = false;
    constexpr std::array<float, 3> kLaneOffsets = { -15.0f, 0.0f, 15.0f };
    const Vector3 playerPosition = boss.player_->GetTranslate();
    const float floorY = boss.hoverCenterY_ - 55.0f;
    size_t hazardIndex = 0;
    for (int32_t lane = 0; lane < 3; ++lane) {
        if (lane == icePillarSafeLane_) continue;
        Vector3 position = {
            boss.hoverCenterX_ + kLaneOffsets[static_cast<size_t>(lane)],
            floorY,
            playerPosition.z + kIcePillarAheadDistance
        };
        icePillarPositions_[hazardIndex++] = position;
        EffectManager::GetInstance()->PlayEffect("IceGroundPattern", position);
    }
}

void IceJellyfishIcePillarAttack::Start(IceJellyfish& boss)
{
    if (boss.player_ == nullptr || icePillarWave_ >= 0) return;
    std::mt19937 randomEngine(std::random_device {}());
    std::uniform_int_distribution<int32_t> laneDistribution(0, 2);
    icePillarSafeLane_ = laneDistribution(randomEngine);
    icePillarWave_ = 0;
    icePillarTimer_ = 0.0f;
    hasHitPlayerByWave_.fill(false);
    PrepareWarning(boss);
}

void IceJellyfishIcePillarAttack::Update(IceJellyfish& boss, float deltaTimeSeconds)
{
    if (icePillarWave_ < 0) return;
    icePillarTimer_ += deltaTimeSeconds;
    if (icePillarModelSpawnedForWave_) {
        icePillarModelTimer_ += deltaTimeSeconds;
    }
    if (!icePillarModelSpawnedForWave_ &&
    icePillarTimer_ >= kIcePillarRevealSeconds) {
        constexpr std::array<float, 3> kSpikeOffsets = { -4.0f, 0.0f, 4.0f };
        size_t modelIndex = 0;
        for (const Vector3& laneCenter : icePillarPositions_) {
            for (size_t spike = 0; spike < kSpikeOffsets.size(); ++spike) {
                float depthOffset = -1.2f;
                if (spike == 1) depthOffset = 1.2f;
                icePillarModelPositions_[modelIndex++] = {
                    laneCenter.x + kSpikeOffsets[spike],
                    laneCenter.y,
                    laneCenter.z + depthOffset
                };
            }
        }
        icePillarModelTimer_ = 0.0f;
        icePillarModelSpawnedForWave_ = true;
    }

    if (boss.player_ != nullptr) {
        const Vector3 playerPosition = boss.player_->GetTranslate();
        if (icePillarModelSpawnedForWave_) {
            for (const Vector3& position : icePillarPositions_) {
                const bool withinWidth = std::abs(playerPosition.x - position.x) <= 6.5f;
                const bool withinDepth = std::abs(playerPosition.z - position.z) <= 4.0f;
                const bool withinHeight = playerPosition.y >= position.y - 2.0f &&
                playerPosition.y <= position.y + 40.0f;
                if (withinWidth && withinDepth && withinHeight) {
                    DamagePlayerOnce(boss, icePillarWave_, 2);
                    break;
                }
            }
        }

        const bool passedPillars = playerPosition.z > icePillarPositions_[0].z + 10.0f;
        if (!passedPillars && icePillarTimer_ < kIcePillarWaveTimeoutSeconds) return;
    } else if (icePillarTimer_ < kIcePillarWaveTimeoutSeconds) {
        return;
    }

    icePillarModelSpawnedForWave_ = false;
    icePillarModelTimer_ = 0.0f;
    ++icePillarWave_;
    icePillarTimer_ = 0.0f;
    if (icePillarWave_ >= 3) {
        icePillarWave_ = -1;
        return;
    }
    icePillarSafeLane_ = (icePillarSafeLane_ + 1 + icePillarWave_ % 2) % 3;
    PrepareWarning(boss);
}

bool IceJellyfishIcePillarAttack::IsTentacleSelected(size_t tentacle) const
{
    return icePillarWave_ >= 0 && static_cast<int32_t>(tentacle % 2) == icePillarWave_ % 2;
}

float IceJellyfishIcePillarAttack::GetCharge() const
{
    return std::clamp(icePillarTimer_ / kIcePillarRevealSeconds, 0.0f, 1.0f);
}

float IceJellyfishIcePillarAttack::GetModelScaleWeight() const
{
    return (std::max)(0.12f, std::clamp(icePillarModelTimer_ / 0.18f, 0.0f, 1.0f));
}

void IceJellyfishIcePillarAttack::DamagePlayerOnce(IceJellyfish& boss, int32_t waveIndex, int damage)
{
    if (boss.player_ == nullptr || waveIndex < 0 || waveIndex >= static_cast<int>(hasHitPlayerByWave_.size()) ||
        hasHitPlayerByWave_[waveIndex]) {
        return;
    }
    hasHitPlayerByWave_[waveIndex] = true;
    if (boss.player_->ApplyDamage(damage)) {
        EffectManager::GetInstance()->PlayEffect("DamageHit", boss.player_->GetTranslate());
    }
}
