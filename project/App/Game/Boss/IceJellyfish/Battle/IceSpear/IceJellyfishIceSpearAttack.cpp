#include "IceJellyfishIceSpearAttack.h"
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

float SmoothStep(float value)
{
    const float weight = std::clamp(value, 0.0f, 1.0f);
    return weight * weight * (3.0f - 2.0f * weight);
}

Vector4 LerpColor(const Vector4& from, const Vector4& to, float weight)
{
    return {
        from.x + (to.x - from.x) * weight,
        from.y + (to.y - from.y) * weight,
        from.z + (to.z - from.z) * weight,
        from.w + (to.w - from.w) * weight
    };
}
}

Vector3 IceJellyfishIceSpearAttack::ApplyTentacleDirection(const IceJellyfish& boss,
    const IceJellyfishAttackMotionContext& context,
    const Vector3& direction) const
{
    if (!IsTentacleSelected(context.tentacleIndex)
        || !boss.IsSegmentAlive(context.tentacleIndex, context.segmentIndex) || boss.player_ == nullptr) {
        return direction;
    }
    Vector3 aimTarget = GetAimTarget(context.tentacleIndex);
    aimTarget.z = boss.player_->GetTranslate().z;
    const Vector3 aimDirection = Normalize((aimTarget - boss.bodyPosition_) - context.jointPosition);
    const float aimWeight = (0.15f + context.tipWeight * 0.67f) * iceSpearPoseWeight_;
    return Normalize(direction * (1.0f - aimWeight) + aimDirection * aimWeight);
}

bool IceJellyfishIceSpearAttack::TryApplyTentacleColor(const IceJellyfish& boss,
    const IceJellyfishAttackMotionContext& context, const Vector4& baseColor, Vector4& color) const
{
    const bool isEmitter = IsEmitter(boss, context.tentacleIndex, context.segmentIndex);
    const bool isChargedSegment = isEmitter ||
        (context.segmentIndex + 1 < IceJellyfish::kSegmentsPerTentacle &&
            IsEmitter(boss, context.tentacleIndex, context.segmentIndex + 1));
    if (!isChargedSegment) {
        return false;
    }
    const Vector4 kChargedColor { 1.65f, 0.45f, 2.80f, 1.0f };
    color = LerpColor(baseColor, kChargedColor, SmoothStep(iceSpearPoseWeight_));
    return true;
}

void IceJellyfishIceSpearAttack::Reset()
{
    lockedTarget_ = {};
    iceSpearTentacles_.fill(-1);
    iceSpearVolley_ = -1;
    iceSpearTimer_ = 0.0f;
    iceSpearPoseWeight_ = 0.0f;
}

void IceJellyfishIceSpearAttack::FireVolley(IceJellyfish& boss)
{
    if (boss.player_ == nullptr || boss.bulletModel_ == nullptr) return;
    if (iceSpearVolley_ < 0 || iceSpearVolley_ >= 3) return;
    const int32_t selectedTentacle = iceSpearTentacles_[iceSpearVolley_];
    if (selectedTentacle < 0) return;
    const size_t tentacle = static_cast<size_t>(selectedTentacle);
    Vector3 target = GetAimTarget(tentacle);
    target.z = boss.player_->GetTranslate().z;

    for (size_t remaining = IceJellyfish::kSegmentsPerTentacle; remaining > 0; --remaining) {
        const size_t segment = remaining - 1;
        if (!boss.IsSegmentAlive(tentacle, segment)) continue;
        const Vector3 muzzle = MatrixMath::Transform(
            {}, boss.tentacles_[tentacle][segment]->GetWorldMatrix());
        EffectManager::GetInstance()->PlayEffect("JellyfishSpearFlash", muzzle);
        const Vector3 targetDirection = Normalize(target - muzzle);
        const float speed = 0.78f + static_cast<float>(iceSpearVolley_) * 0.06f;
        constexpr int32_t kSpearCount = 5;
        for (int32_t spear = 0; spear < kSpearCount; ++spear) {
            Vector3 direction = targetDirection;
            if (spear > 0) {
                const float angle = kTwoPi * static_cast<float>(spear - 1) /
                    static_cast<float>(kSpearCount - 1);
                direction = Normalize(targetDirection + Vector3 {
                    std::cos(angle) * 0.055f,
                    std::sin(angle) * 0.055f,
                    0.0f });
            }

            auto bullet = std::make_unique<NormalEnemyBullet>();
            bullet->Initialize(boss.bulletModel_);
            bullet->SetTranslate(muzzle);
            bullet->SetVelocity(direction * speed);
            bullet->SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
            Vector3 bulletScale { 0.30f, 0.30f, 1.25f };
            int bulletDamage = 1;
            if (spear == 0) {
                bulletScale = { 0.44f, 0.44f, 1.70f };
                bulletDamage = 2;
            }
            bullet->SetScale(bulletScale);
            bullet->SetDamage(bulletDamage);
            boss.AddEnemyBullet(std::move(bullet));
        }
        break;
    }
}

bool IceJellyfishIceSpearAttack::IsEmitter(const IceJellyfish& boss, size_t tentacle, size_t segment) const
{
    if (iceSpearVolley_ < 0 || !boss.IsSegmentAlive(tentacle, segment)) return false;
    if (!IsTentacleSelected(tentacle)) return false;
    return segment + 1 == IceJellyfish::kSegmentsPerTentacle || !boss.IsSegmentAlive(tentacle, segment + 1);
}

bool IceJellyfishIceSpearAttack::IsTentacleSelected(size_t tentacle) const
{
    if (iceSpearVolley_ < 0) return false;
    for (int32_t selectedTentacle : iceSpearTentacles_) {
        if (selectedTentacle == static_cast<int32_t>(tentacle)) {
            return true;
        }
    }
    return false;
}

Vector3 IceJellyfishIceSpearAttack::GetAimTarget(size_t tentacle) const
{
    constexpr std::array<Vector3, 3> kTargetOffsets = {
        Vector3 { -14.0f, 0.0f, 0.0f },
        Vector3 { 14.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 0.0f }
    };
    for (size_t index = 0; index < iceSpearTentacles_.size(); ++index) {
        if (iceSpearTentacles_[index] == static_cast<int32_t>(tentacle)) {
            return lockedTarget_ + kTargetOffsets[index];
        }
    }
    return lockedTarget_;
}

void IceJellyfishIceSpearAttack::Start(IceJellyfish& boss)
{
    iceSpearTentacles_.fill(-1);
    std::array<int32_t, IceJellyfish::kTentacleCount> candidates {};
    size_t candidateCount = 0;
    for (size_t tentacle = 0; tentacle < IceJellyfish::kTentacleCount; ++tentacle) {
        if (boss.IsSegmentAlive(tentacle, 0)) {
            candidates[candidateCount++] = static_cast<int32_t>(tentacle);
        }
    }

    std::mt19937 randomEngine(std::random_device {}());
    std::shuffle(candidates.begin(), candidates.begin() + candidateCount, randomEngine);
    const size_t selectedCount = (std::min)(iceSpearTentacles_.size(), candidateCount);
    for (size_t index = 0; index < selectedCount; ++index) {
        iceSpearTentacles_[index] = candidates[index];
    }
    if (boss.player_ != nullptr) lockedTarget_ = boss.player_->GetTranslate();
    iceSpearVolley_ = -1;
    if (selectedCount > 0) {
        iceSpearVolley_ = 0;
    }
    iceSpearTimer_ = 0.0f;
    iceSpearPoseWeight_ = 0.0f;
}

void IceJellyfishIceSpearAttack::Update(IceJellyfish& boss, float deltaTimeSeconds)
{
    if (iceSpearVolley_ >= 0) {
        iceSpearTimer_ += deltaTimeSeconds;
        if (iceSpearVolley_ < 3) {
            iceSpearPoseWeight_ = (std::min)(1.0f, iceSpearPoseWeight_ + deltaTimeSeconds * 2.8f);
            while (iceSpearVolley_ < 3 &&
            iceSpearTimer_ >= 1.0f + static_cast<float>(iceSpearVolley_) * 0.20f) {
                FireVolley(boss);
                ++iceSpearVolley_;
            }
        }
        if (iceSpearVolley_ >= 3) {
            iceSpearPoseWeight_ = (std::max)(0.0f, iceSpearPoseWeight_ - deltaTimeSeconds * 1.8f);
            if (iceSpearPoseWeight_ <= 0.0f) {
                iceSpearVolley_ = -1;
                iceSpearTentacles_.fill(-1);
            }
        }
    }

}
