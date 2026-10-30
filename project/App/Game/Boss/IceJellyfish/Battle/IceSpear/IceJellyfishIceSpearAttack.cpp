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
// 調整用の設定値。意味が異なる値は、数値が同じでも個別に保持する。
constexpr float kAimBaseWeight = 0.15f;
constexpr float kAimTipWeight = 0.67f;
constexpr float kBulletBaseSpeed = 0.78f;
constexpr float kBulletSpeedPerVolley = 0.06f;
constexpr float kBulletSpread = 0.055f;
constexpr Vector4 kBulletColor = { 1.0f, 0.0f, 0.0f, 1.0f };
constexpr Vector3 kBulletScale = { 0.30f, 0.30f, 1.25f };
constexpr Vector3 kCenterBulletScale = { 0.44f, 0.44f, 1.70f };
constexpr int kBulletDamage = 1;
constexpr int kCenterBulletDamage = 2;
constexpr float kPoseChargeRate = 2.8f;
constexpr float kPoseReleaseRate = 1.8f;
constexpr float kFirstVolleyDelaySeconds = 1.0f;
constexpr float kVolleyIntervalSeconds = 0.20f;

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
        || !boss.IsSegmentAlive(context.tentacleIndex, context.segmentIndex) || !boss.HasAttackTarget()) {
        return direction;
    }
    Vector3 aimTarget = GetAimTarget(context.tentacleIndex);
    aimTarget.z = boss.GetAttackTargetPosition().z;
    const Vector3 aimDirection = Normalize((aimTarget - boss.GetAttackBodyPosition()) - context.jointPosition);
    const float aimWeight = (kAimBaseWeight + context.tipWeight * kAimTipWeight) * iceSpearPoseWeight_;
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
    constexpr Vector4 kChargedColor { 1.65f, 0.45f, 2.80f, 1.0f };
    color = LerpColor(baseColor, kChargedColor, SmoothStep(iceSpearPoseWeight_));
    return true;
}

void IceJellyfishIceSpearAttack::Reset()
{
    pendingVolleyCount_ = 0;
    lockedTarget_ = {};
    iceSpearTentacles_.fill(-1);
    iceSpearVolley_ = -1;
    iceSpearTimer_ = 0.0f;
    iceSpearPoseWeight_ = 0.0f;
}

void IceJellyfishIceSpearAttack::FireVolley(IceJellyfish& boss, const PendingVolley& shot)
{
    if (!boss.HasAttackTarget() || boss.GetAttackBulletModel() == nullptr) return;
    const int32_t selectedTentacle = shot.tentacle;
    if (selectedTentacle < 0) return;
    const size_t tentacle = static_cast<size_t>(selectedTentacle);
    const Vector3 target = shot.target;

    for (size_t remaining = IceJellyfish::kSegmentsPerTentacle; remaining > 0; --remaining) {
        const size_t segment = remaining - 1;
        if (!boss.IsSegmentAlive(tentacle, segment)) continue;
        const Vector3 muzzle = boss.GetAttackSegmentPosition(tentacle, segment);
        EffectManager::GetInstance()->PlayEffect("JellyfishSpearFlash", muzzle);
        const Vector3 targetDirection = Normalize(target - muzzle);
        const float speed = kBulletBaseSpeed + static_cast<float>(shot.volley) * kBulletSpeedPerVolley;
        constexpr int32_t kSpearCount = 5;
        for (int32_t spear = 0; spear < kSpearCount; ++spear) {
            Vector3 direction = targetDirection;
            if (spear > 0) {
                const float angle = kTwoPi * static_cast<float>(spear - 1) /
                    static_cast<float>(kSpearCount - 1);
                direction = Normalize(targetDirection + Vector3 {
                    std::cos(angle) * kBulletSpread,
                    std::sin(angle) * kBulletSpread,
                    0.0f });
            }

            auto bullet = std::make_unique<NormalEnemyBullet>();
            bullet->Initialize(boss.GetAttackBulletModel());
            bullet->SetTranslate(muzzle);
            bullet->SetVelocity(direction * speed);
            bullet->SetColor(kBulletColor);
            Vector3 bulletScale = kBulletScale;
            int bulletDamage = kBulletDamage;
            if (spear == 0) {
                bulletScale = kCenterBulletScale;
                bulletDamage = kCenterBulletDamage;
            }
            bullet->SetScale(bulletScale);
            bullet->SetDamage(bulletDamage);
            boss.SpawnAttackBullet(std::move(bullet));
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
    if (boss.HasAttackTarget()) lockedTarget_ = boss.GetAttackTargetPosition();
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
            iceSpearPoseWeight_ = (std::min)(1.0f, iceSpearPoseWeight_ + deltaTimeSeconds * kPoseChargeRate);
            while (iceSpearVolley_ < 3 &&
            iceSpearTimer_ >= kFirstVolleyDelaySeconds + static_cast<float>(iceSpearVolley_) * kVolleyIntervalSeconds) {
                const int32_t tentacle = iceSpearTentacles_[iceSpearVolley_];
                if (tentacle >= 0 && pendingVolleyCount_ < pendingVolleys_.size()) {
                    Vector3 target = GetAimTarget(static_cast<size_t>(tentacle));
                    target.z = boss.GetAttackTargetPosition().z;
                    pendingVolleys_[pendingVolleyCount_++] = { iceSpearVolley_, tentacle, target };
                }
                ++iceSpearVolley_;
            }
        }
        if (iceSpearVolley_ >= 3) {
            iceSpearPoseWeight_ = (std::max)(0.0f, iceSpearPoseWeight_ - deltaTimeSeconds * kPoseReleaseRate);
            if (iceSpearPoseWeight_ <= 0.0f) {
                iceSpearVolley_ = -1;
                iceSpearTentacles_.fill(-1);
            }
        }
    }

}

void IceJellyfishIceSpearAttack::EmitPendingVolleys(IceJellyfish& boss)
{
    // 姿勢確定後にだけ呼び、発射位置と描画位置を一致させる。
    for (size_t index = 0; index < pendingVolleyCount_; ++index) {
        FireVolley(boss, pendingVolleys_[index]);
    }
    pendingVolleyCount_ = 0;
}
