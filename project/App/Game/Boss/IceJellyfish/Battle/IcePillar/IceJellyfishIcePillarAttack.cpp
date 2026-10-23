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
// 調整用の設定値。意味が異なる値は、数値が同じでも個別に保持する。
constexpr float kTentacleRadialLean = 0.08f;
constexpr float kPointDownBaseWeight = 0.35f;
constexpr float kPointDownTipWeight = 0.25f;
constexpr Vector3 kChargeBaseColor = { 0.55f, 0.80f, 1.00f };
constexpr Vector3 kChargeColorGain = { 0.65f, 1.35f, 1.70f };
constexpr float kFloorOffsetBelowHover = 55.0f;
constexpr float kOuterSpikeDepthOffset = -1.2f;
constexpr float kCenterSpikeDepthOffset = 1.2f;
constexpr float kCollisionHalfWidth = 6.5f;
constexpr float kCollisionHalfDepth = 4.0f;
constexpr float kCollisionBottomOffset = 2.0f;
constexpr float kCollisionTopOffset = 40.0f;
constexpr int kDamage = 2;
constexpr float kPassedPillarDistance = 10.0f;
constexpr float kMinimumModelScale = 0.12f;
constexpr float kModelGrowthSeconds = 0.18f;

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
        context.radialX * kTentacleRadialLean, -1.0f, context.radialZ * kTentacleRadialLean
    });
    const float pointDownWeight = GetCharge() * (kPointDownBaseWeight + context.tipWeight * kPointDownTipWeight);
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
        kChargeBaseColor.x + charge * kChargeColorGain.x,
        kChargeBaseColor.y + charge * kChargeColorGain.y,
        kChargeBaseColor.z + charge * kChargeColorGain.z,
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
    if (!boss.HasAttackTarget() || icePillarWave_ < 0 || icePillarWave_ >= 3) return;
    icePillarModelSpawnedForWave_ = false;
    constexpr std::array<float, 3> kLaneOffsets = { -15.0f, 0.0f, 15.0f };
    const Vector3 playerPosition = boss.GetAttackTargetPosition();
    const float floorY = boss.GetAttackHoverCenter().y - kFloorOffsetBelowHover;
    size_t hazardIndex = 0;
    for (int32_t lane = 0; lane < 3; ++lane) {
        if (lane == icePillarSafeLane_) continue;
        Vector3 position = {
            boss.GetAttackHoverCenter().x + kLaneOffsets[static_cast<size_t>(lane)],
            floorY,
            playerPosition.z + kIcePillarAheadDistance
        };
        icePillarPositions_[hazardIndex++] = position;
        EffectManager::GetInstance()->PlayEffect("IceGroundPattern", position);
    }
}

void IceJellyfishIcePillarAttack::Start(IceJellyfish& boss)
{
    if (!boss.HasAttackTarget() || icePillarWave_ >= 0) return;
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
                float depthOffset = kOuterSpikeDepthOffset;
                if (spike == 1) depthOffset = kCenterSpikeDepthOffset;
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

    if (boss.HasAttackTarget()) {
        const Vector3 playerPosition = boss.GetAttackTargetPosition();
        if (icePillarModelSpawnedForWave_) {
            for (const Vector3& position : icePillarPositions_) {
                const bool withinWidth = std::abs(playerPosition.x - position.x) <= kCollisionHalfWidth;
                const bool withinDepth = std::abs(playerPosition.z - position.z) <= kCollisionHalfDepth;
                const bool withinHeight = playerPosition.y >= position.y - kCollisionBottomOffset &&
                playerPosition.y <= position.y + kCollisionTopOffset;
                if (withinWidth && withinDepth && withinHeight) {
                    DamagePlayerOnce(boss, icePillarWave_, kDamage);
                    break;
                }
            }
        }

        const bool passedPillars = playerPosition.z > icePillarPositions_[0].z + kPassedPillarDistance;
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
    return (std::max)(kMinimumModelScale, std::clamp(icePillarModelTimer_ / kModelGrowthSeconds, 0.0f, 1.0f));
}

void IceJellyfishIcePillarAttack::DamagePlayerOnce(IceJellyfish& boss, int32_t waveIndex, int damage)
{
    if (!boss.HasAttackTarget() || waveIndex < 0 || waveIndex >= static_cast<int>(hasHitPlayerByWave_.size()) ||
        hasHitPlayerByWave_[waveIndex]) {
        return;
    }
    hasHitPlayerByWave_[waveIndex] = true;
    if (boss.ApplyAttackDamageToPlayer(damage)) {
        EffectManager::GetInstance()->PlayEffect("DamageHit", boss.GetAttackTargetPosition());
    }
}
