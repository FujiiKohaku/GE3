#include "IceJellyfish.h"
#include "IceJellyfishCollision.h"

#include "App/Game/Enemy/Bullet/NormalEnemyBullet.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/ModelManager.h"
#include "Engine/3D/Object3dManager.h"
#include "Engine/Debug/DebugRenderer.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/math/MatrixMath.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace {
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
constexpr float kIcePillarAheadDistance = 190.0f;
constexpr float kIcePillarRevealSeconds = 2.00f;
constexpr float kIcePillarWaveTimeoutSeconds = 9.00f;

bool Crossed(float previous, float current, float point)
{
    return previous < point && current >= point;
}
}

std::unique_ptr<Object3d> IceJellyfish::CreatePart(
    Camera* camera, const char* modelPath, const Vector4& color, bool ice)
{
    auto object = std::make_unique<Object3d>();
    object->Initialize(Object3dManager::GetInstance());
    object->SetModel(ModelManager::GetInstance()->Load(modelPath));
    object->SetCamera(camera);
    object->SetColor(color);
    object->SetEnableLighting(ice);
    object->SetEnableEnvironmentMap(false);
    object->SetEnvironmentMapStrength(0.0f);
    object->GetMaterial()->shininess = 0.0f;
    return object;
}

void IceJellyfish::Initialize(Camera* camera, Model* bulletModel, Player* player)
{
    camera_ = camera;
    bulletModel_ = bulletModel;
    player_ = player;
    hp_ = kMaxHp;
    isDead_ = false;
    animationTime_ = 0.0f;
    deathTimer_ = 0.0f;
    hitFlashTime_ = 0.0f;
    absoluteZeroUsed_ = false;
    pulseAttackArmed_ = false;
    pulseBarrageWave_ = -1;
    pulseBarrageTimer_ = 0.0f;
    iceSpearTentacles_.fill(-1);
    iceSpearVolley_ = -1;
    iceSpearTimer_ = 0.0f;
    iceSpearPoseWeight_ = 0.0f;
    icePillarWave_ = -1;
    icePillarTimer_ = 0.0f;
    icePillarModelTimer_ = 0.0f;
    icePillarModelSpawnedForWave_ = false;
    nextAttackIndex_ = 0;
    patternIndex_ = 0;
    phase_ = 1;

    bell_ = CreatePart(camera, "Boss/IceJellyfish/IceJellyfishBell.obj",
        { 0.65f, 0.86f, 1.0f, 1.0f }, true);
    core_ = CreatePart(camera, "Boss/IceJellyfish/IceJellyfishCore.obj",
        { 2.8f, 1.25f, 0.25f, 1.0f }, false);

    std::mt19937 randomEngine(std::random_device {}());
    std::uniform_real_distribution<float> phaseDistribution(0.0f, kTwoPi);
    std::uniform_real_distribution<float> speedDistribution(0.95f, 1.35f);
    std::uniform_real_distribution<float> amplitudeDistribution(0.82f, 1.25f);
    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        tentacleHp_[tentacle] = kTentacleMaxHp;
        tentacleWavePhase_[tentacle] = phaseDistribution(randomEngine);
        tentacleWaveSpeed_[tentacle] = speedDistribution(randomEngine);
        tentacleWaveAmplitude_[tentacle] = amplitudeDistribution(randomEngine);
        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            const bool isTip = segment + 1 == kSegmentsPerTentacle;
            tentacles_[tentacle][segment] = CreatePart(camera,
                isTip ? "Boss/IceJellyfish/IceJellyfishTip.obj"
                      : "Boss/IceJellyfish/IceJellyfishSegment.obj",
                { 0.55f, 0.80f, 1.0f, 1.0f }, true);
        }
    }

    for (size_t crystal = 0; crystal < crystals_.size(); ++crystal) {
        crystals_[crystal] = CreatePart(camera, "Environment/Ice/IceSpike.obj",
            { 0.35f, 0.82f, 1.0f, 1.0f }, true);
        crystalHp_[crystal] = 0.0f;
    }
    for (std::unique_ptr<Object3d>& pillar : icePillarModels_) {
        pillar = CreatePart(camera, "Environment/Ice/IceSpike.obj",
            { 1.1f, 1.65f, 2.6f, 1.0f }, false);
    }

    tentacleColliders_.reserve(kTentacleCount * kSegmentsPerTentacle);
    tentacleColliderParts_.reserve(kTentacleCount * kSegmentsPerTentacle);
    BeginPattern(AttackPattern::FrozenSweep);
    UpdatePartTransforms();
}

void IceJellyfish::SetPosition(const Vector3& position)
{
    basePosition_ = position;
    bodyPosition_ = position;
    hoverCenterX_ = position.x;
    hoverCenterY_ = position.y;
    transform_.translate = position;
    UpdatePartTransforms();
}

void IceJellyfish::Update()
{
    const float deltaTime = TimeManager::GetInstance()->GetDeltaTime();
    animationTime_ += (std::max)(deltaTime, 0.0f);
    hitFlashTime_ = (std::max)(0.0f, hitFlashTime_ - deltaTime);

    if (isDead_) {
        deathTimer_ += deltaTime;
        bodyPosition_.y -= 9.0f * deltaTime;
        bodyPosition_.z += 5.0f * deltaTime;
        UpdatePartTransforms();
        return;
    }

    if (player_ != nullptr) {
        const Vector3 playerPosition = player_->GetTranslate();
        bodyPosition_ = {
            hoverCenterX_ + std::sin(animationTime_ * 0.48f) * 8.0f,
            hoverCenterY_ + std::sin(animationTime_ * 1.10f) * 5.0f,
            playerPosition.z + kHoverDistance
        };
    } else {
        bodyPosition_ = basePosition_;
    }
    basePosition_ = bodyPosition_;
    transform_.translate = bodyPosition_;

    coreExposed_ = AreAllTentaclesDestroyed();
    UpdateAttackSequence(animationTime_ * 1.10f, deltaTime);
    coreExposed_ = AreAllTentaclesDestroyed();
    UpdatePartTransforms();
}

void IceJellyfish::UpdateAttackSequence(float floatPhase, float deltaTime)
{
    if (pulseBarrageWave_ >= 0) {
        pulseBarrageTimer_ += deltaTime;
        while (pulseBarrageWave_ < 3 &&
            pulseBarrageTimer_ >= static_cast<float>(pulseBarrageWave_) * 0.18f) {
            int32_t aliveTentacles = 0;
            for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
                if (IsSegmentAlive(tentacle, 0)) ++aliveTentacles;
            }

            if (pulseBarrageWave_ == 0) {
                FirePulseRing(aliveTentacles * 2, 0.12f, 0.65f, 0.0f, false);
            } else if (pulseBarrageWave_ == 1) {
                const int count = aliveTentacles * 2;
                const float halfStep = count > 0 ? std::numbers::pi_v<float> /
                    static_cast<float>(count) : 0.0f;
                FirePulseRing(count, 0.22f, 0.62f, halfStep, false);
            } else {
                FirePulseRing(aliveTentacles, 0.08f, 0.82f,
                    std::numbers::pi_v<float> * 0.5f, true);
            }
            ++pulseBarrageWave_;
        }
        if (pulseBarrageWave_ >= 3) pulseBarrageWave_ = -1;
    }

    if (iceSpearVolley_ >= 0) {
        iceSpearTimer_ += deltaTime;
        if (iceSpearVolley_ < 3) {
            iceSpearPoseWeight_ = (std::min)(1.0f, iceSpearPoseWeight_ + deltaTime * 2.8f);
            while (iceSpearVolley_ < 3 &&
                iceSpearTimer_ >= 1.0f + static_cast<float>(iceSpearVolley_) * 0.20f) {
                FireIceSpearVolley();
                ++iceSpearVolley_;
            }
        }
        if (iceSpearVolley_ >= 3) {
            iceSpearPoseWeight_ = (std::max)(0.0f, iceSpearPoseWeight_ - deltaTime * 1.8f);
            if (iceSpearPoseWeight_ <= 0.0f) {
                iceSpearVolley_ = -1;
                iceSpearTentacles_.fill(-1);
            }
        }
    }

    UpdateIcePillarAttack(deltaTime);

    const float pulse = std::sin(floatPhase);
    if (pulse <= -0.72f) {
        pulseAttackArmed_ = true;
    }
    if (!pulseAttackArmed_ || pulse < -0.05f) {
        return;
    }

    if (nextAttackIndex_ == 0) {
        pulseBarrageWave_ = 0;
        pulseBarrageTimer_ = 0.0f;
    } else if (nextAttackIndex_ == 1) {
        StartIceSpearAttack();
    } else {
        StartIcePillarAttack();
    }
    nextAttackIndex_ = (nextAttackIndex_ + 1) % 3;
    pulseAttackArmed_ = false;
}

void IceJellyfish::StartIceSpearAttack()
{
    iceSpearTentacles_.fill(-1);
    std::array<int32_t, kTentacleCount> candidates {};
    size_t candidateCount = 0;
    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        if (IsSegmentAlive(tentacle, 0)) {
            candidates[candidateCount++] = static_cast<int32_t>(tentacle);
        }
    }

    std::mt19937 randomEngine(std::random_device {}());
    std::shuffle(candidates.begin(), candidates.begin() + candidateCount, randomEngine);
    const size_t selectedCount = (std::min)(iceSpearTentacles_.size(), candidateCount);
    for (size_t index = 0; index < selectedCount; ++index) {
        iceSpearTentacles_[index] = candidates[index];
    }
    if (player_ != nullptr) lockedTarget_ = player_->GetTranslate();
    iceSpearVolley_ = selectedCount > 0 ? 0 : -1;
    iceSpearTimer_ = 0.0f;
    iceSpearPoseWeight_ = 0.0f;
}

void IceJellyfish::FireIceSpearVolley()
{
    if (player_ == nullptr || bulletModel_ == nullptr) return;
    if (iceSpearVolley_ < 0 || iceSpearVolley_ >= 3) return;
    const int32_t selectedTentacle = iceSpearTentacles_[iceSpearVolley_];
    if (selectedTentacle < 0) return;
    const size_t tentacle = static_cast<size_t>(selectedTentacle);
    Vector3 target = GetIceSpearAimTarget(tentacle);
    target.z = player_->GetTranslate().z;

    for (size_t remaining = kSegmentsPerTentacle; remaining > 0; --remaining) {
        const size_t segment = remaining - 1;
        if (!IsSegmentAlive(tentacle, segment)) continue;
        const Vector3 muzzle = MatrixMath::Transform(
            {}, tentacles_[tentacle][segment]->GetWorldMatrix());
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
            bullet->Initialize(bulletModel_);
            bullet->SetTranslate(muzzle);
            bullet->SetVelocity(direction * speed);
            bullet->SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
            bullet->SetScale(spear == 0
                ? Vector3 { 0.44f, 0.44f, 1.70f }
                : Vector3 { 0.30f, 0.30f, 1.25f });
            bullet->SetDamage(spear == 0 ? 2 : 1);
            AddEnemyBullet(std::move(bullet));
        }
        break;
    }
}

void IceJellyfish::StartIcePillarAttack()
{
    if (player_ == nullptr || icePillarWave_ >= 0) return;
    std::mt19937 randomEngine(std::random_device {}());
    std::uniform_int_distribution<int32_t> laneDistribution(0, 2);
    icePillarSafeLane_ = laneDistribution(randomEngine);
    icePillarWave_ = 0;
    icePillarTimer_ = 0.0f;
    for (int32_t slot = 3; slot < 6; ++slot) attackHitApplied_[slot] = false;
    PrepareIcePillarWarning();
}

void IceJellyfish::PrepareIcePillarWarning()
{
    if (player_ == nullptr || icePillarWave_ < 0 || icePillarWave_ >= 3) return;
    icePillarModelSpawnedForWave_ = false;
    constexpr std::array<float, 3> laneOffsets = { -15.0f, 0.0f, 15.0f };
    const Vector3 playerPosition = player_->GetTranslate();
    const float floorY = hoverCenterY_ - 55.0f;
    size_t hazardIndex = 0;
    for (int32_t lane = 0; lane < 3; ++lane) {
        if (lane == icePillarSafeLane_) continue;
        Vector3 position = {
            hoverCenterX_ + laneOffsets[static_cast<size_t>(lane)],
            floorY,
            playerPosition.z + kIcePillarAheadDistance
        };
        icePillarPositions_[hazardIndex++] = position;
        EffectManager::GetInstance()->PlayEffect("IceGroundPattern", position);
    }
}

void IceJellyfish::UpdateIcePillarAttack(float deltaTime)
{
    if (icePillarWave_ < 0) return;
    icePillarTimer_ += deltaTime;
    if (icePillarModelSpawnedForWave_) {
        icePillarModelTimer_ += deltaTime;
    }
    if (!icePillarModelSpawnedForWave_ &&
        icePillarTimer_ >= kIcePillarRevealSeconds) {
        constexpr std::array<float, 3> spikeOffsets = { -4.0f, 0.0f, 4.0f };
        size_t modelIndex = 0;
        for (const Vector3& laneCenter : icePillarPositions_) {
            for (size_t spike = 0; spike < spikeOffsets.size(); ++spike) {
                icePillarModelPositions_[modelIndex++] = {
                    laneCenter.x + spikeOffsets[spike],
                    laneCenter.y,
                    laneCenter.z + (spike == 1 ? 1.2f : -1.2f)
                };
            }
        }
        icePillarModelTimer_ = 0.0f;
        icePillarModelSpawnedForWave_ = true;
    }

    if (player_ != nullptr) {
        const Vector3 playerPosition = player_->GetTranslate();
        if (icePillarModelSpawnedForWave_) {
            for (const Vector3& position : icePillarPositions_) {
                const bool withinWidth = std::abs(playerPosition.x - position.x) <= 6.5f;
                const bool withinDepth = std::abs(playerPosition.z - position.z) <= 4.0f;
                const bool withinHeight = playerPosition.y >= position.y - 2.0f &&
                    playerPosition.y <= position.y + 40.0f;
                if (withinWidth && withinDepth && withinHeight) {
                    DamagePlayerOnce(3 + icePillarWave_, 2);
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
    PrepareIcePillarWarning();
}

void IceJellyfish::BeginPattern(AttackPattern pattern)
{
    attackPattern_ = pattern;
    attackTimer_ = 0.0f;
    previousAttackTimer_ = 0.0f;
    firedWave_ = 0;
    coreExposed_ = AreAllTentaclesDestroyed();
    absoluteInterrupted_ = false;
    attackHitApplied_.fill(false);
    absoluteSealDamage_.fill(0.0f);
    if (player_ != nullptr) {
        lockedTarget_ = player_->GetTranslate();
    }
    if (pattern == AttackPattern::CrystalPrison) {
        crystalHp_.fill(6.0f);
    } else {
        crystalHp_.fill(0.0f);
    }
}

void IceJellyfish::SelectNextPattern()
{
    if (phase_ == 1) {
        constexpr std::array<AttackPattern, 3> patterns = {
            AttackPattern::FrozenSweep,
            AttackPattern::IcicleBloom,
            AttackPattern::TentacleThrust
        };
        patternIndex_ = (patternIndex_ + 1) % static_cast<int32_t>(patterns.size());
        BeginPattern(patterns[patternIndex_]);
        return;
    }

    constexpr std::array<AttackPattern, 5> patterns = {
        AttackPattern::FrozenSweep,
        AttackPattern::BlizzardCurrent,
        AttackPattern::TentacleThrust,
        AttackPattern::CrystalPrison,
        AttackPattern::IcicleBloom
    };
    patternIndex_ = (patternIndex_ + 1) % static_cast<int32_t>(patterns.size());
    BeginPattern(patterns[patternIndex_]);
}

float IceJellyfish::PatternDuration() const
{
    switch (attackPattern_) {
    case AttackPattern::FrozenSweep: return phase_ == 3 ? 5.4f : 6.0f;
    case AttackPattern::IcicleBloom: return 6.2f;
    case AttackPattern::TentacleThrust: return 6.5f;
    case AttackPattern::BlizzardCurrent: return 6.5f;
    case AttackPattern::CrystalPrison: return 7.5f;
    case AttackPattern::AbsoluteZero: return 8.0f;
    }
    return 6.0f;
}

void IceJellyfish::UpdatePattern(float deltaTime)
{
    previousAttackTimer_ = attackTimer_;
    attackTimer_ += deltaTime;

    switch (attackPattern_) {
    case AttackPattern::FrozenSweep: UpdateFrozenSweep(); break;
    case AttackPattern::IcicleBloom: UpdateIcicleBloom(); break;
    case AttackPattern::TentacleThrust: UpdateTentacleThrust(); break;
    case AttackPattern::BlizzardCurrent: UpdateBlizzardCurrent(); break;
    case AttackPattern::CrystalPrison: UpdateCrystalPrison(); break;
    case AttackPattern::AbsoluteZero: UpdateAbsoluteZero(); break;
    }

    if (attackTimer_ >= PatternDuration()) {
        SelectNextPattern();
    }
}

void IceJellyfish::DamagePlayerOnce(int slot, int damage)
{
    if (player_ == nullptr || slot < 0 || slot >= static_cast<int>(attackHitApplied_.size()) ||
        attackHitApplied_[slot]) {
        return;
    }
    attackHitApplied_[slot] = true;
    if (player_->ApplyDamage(damage)) {
        EffectManager::GetInstance()->PlayEffect("DamageHit", player_->GetTranslate());
    }
}

void IceJellyfish::UpdateFrozenSweep()
{
    if (Crossed(previousAttackTimer_, attackTimer_, 0.8f) && player_ != nullptr) {
        lockedTarget_ = player_->GetTranslate();
    }
    if (Crossed(previousAttackTimer_, attackTimer_, 2.0f) && player_ != nullptr &&
        std::abs(player_->GetTranslate().y - lockedTarget_.y) < 6.0f) {
        DamagePlayerOnce(0, 2);
    }
    if (phase_ == 3 && Crossed(previousAttackTimer_, attackTimer_, 3.0f) && player_ != nullptr &&
        std::abs(player_->GetTranslate().x - lockedTarget_.x) < 6.0f) {
        DamagePlayerOnce(1, 2);
    }
    coreExposed_ = AreAllTentaclesDestroyed();
}

void IceJellyfish::UpdateIcicleBloom()
{
    const int32_t waves = phase_ == 1 ? 2 : 3;
    while (firedWave_ < waves) {
        const float fireTime = 1.4f + static_cast<float>(firedWave_) * 0.9f;
        if (!Crossed(previousAttackTimer_, attackTimer_, fireTime)) break;
        FireRadialBurst(8, phase_ == 3 ? 0.72f : 0.58f, 1,
            static_cast<float>(firedWave_) * std::numbers::pi_v<float> / 8.0f);
        ++firedWave_;
    }
    coreExposed_ = AreAllTentaclesDestroyed();
}

void IceJellyfish::UpdateTentacleThrust()
{
    if (Crossed(previousAttackTimer_, attackTimer_, 0.9f) && player_ != nullptr) {
        lockedTarget_ = player_->GetTranslate();
    }
    const int32_t thrustCount = phase_ == 1 ? 2 : 3;
    for (int32_t thrust = 0; thrust < thrustCount; ++thrust) {
        const float hitTime = 1.8f + static_cast<float>(thrust) * 0.85f;
        if (Crossed(previousAttackTimer_, attackTimer_, hitTime) && player_ != nullptr) {
            const Vector3 difference = player_->GetTranslate() - lockedTarget_;
            if (Dot(difference, difference) <= 64.0f) DamagePlayerOnce(thrust, 2);
            lockedTarget_ = player_->GetTranslate();
        }
    }
    coreExposed_ = AreAllTentaclesDestroyed();
}

void IceJellyfish::UpdateBlizzardCurrent()
{
    if (player_ != nullptr && attackTimer_ >= 1.0f && attackTimer_ < 5.2f) {
        const float direction = (patternIndex_ % 2 == 0) ? 1.0f : -1.0f;
        player_->ApplyRailAreaForce({ direction * (phase_ == 3 ? 0.13f : 0.09f), 0.0f, 0.0f });
    }
    while (firedWave_ < 5) {
        const float fireTime = 1.3f + static_cast<float>(firedWave_) * 0.75f;
        if (!Crossed(previousAttackTimer_, attackTimer_, fireTime)) break;
        FireAimedBurst(3, 0.24f, 0.42f, 1);
        ++firedWave_;
    }
    coreExposed_ = AreAllTentaclesDestroyed();
}

void IceJellyfish::UpdateCrystalPrison()
{
    UpdateCrystalTransforms();
    int32_t alive = 0;
    for (float hp : crystalHp_) {
        if (hp > 0.0f) ++alive;
    }
    if (Crossed(previousAttackTimer_, attackTimer_, 5.2f) && alive > 0 && player_ != nullptr) {
        const Vector3 offset = player_->GetTranslate() - lockedTarget_;
        if (std::abs(offset.x) > 10.0f || std::abs(offset.y) > 10.0f) {
            DamagePlayerOnce(0, 3);
        }
    }
    if (Crossed(previousAttackTimer_, attackTimer_, 5.6f) && alive > 0) {
        FireAimedBurst(alive, 0.18f, 0.60f, 1);
    }
    coreExposed_ = AreAllTentaclesDestroyed();
}

void IceJellyfish::UpdateAbsoluteZero()
{
    int32_t released = 0;
    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        if (!IsSegmentAlive(tentacle, kSegmentsPerTentacle - 1) ||
            absoluteSealDamage_[tentacle] >= 3.0f) {
            ++released;
        }
    }
    absoluteInterrupted_ = released == static_cast<int32_t>(kTentacleCount);
    coreExposed_ = AreAllTentaclesDestroyed();
    if (!absoluteInterrupted_ && Crossed(previousAttackTimer_, attackTimer_, 6.0f)) {
        DamagePlayerOnce(0, 4);
        FireRadialBurst(16, 0.78f, 2, 0.0f);
    }
}

void IceJellyfish::FireAimedBurst(int count, float spread, float speed, int damage)
{
    if (player_ == nullptr || bulletModel_ == nullptr || count <= 0) return;
    const Vector3 muzzle = bodyPosition_ + Vector3 { 0.0f, -2.0f, -15.0f };
    const Vector3 targetDirection = Normalize(player_->GetTranslate() - muzzle);
    for (int32_t index = 0; index < count; ++index) {
        const float centered = static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f;
        auto bullet = std::make_unique<NormalEnemyBullet>();
        bullet->Initialize(bulletModel_);
        bullet->SetTranslate(muzzle);
        bullet->SetVelocity(Normalize(targetDirection + Vector3 { centered * spread, 0.0f, 0.0f }) * speed);
        bullet->SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
        bullet->SetScale({ 0.45f, 0.45f, 0.8f });
        bullet->SetDamage(damage);
        AddEnemyBullet(std::move(bullet));
    }
}

void IceJellyfish::FireRadialBurst(int count, float speed, int damage, float angleOffset)
{
    if (bulletModel_ == nullptr || count <= 0) return;
    const Vector3 muzzle = bodyPosition_ + Vector3 { 0.0f, -2.0f, -15.0f };
    for (int32_t index = 0; index < count; ++index) {
        const float angle = angleOffset + kTwoPi * static_cast<float>(index) / static_cast<float>(count);
        const Vector3 direction = Normalize(Vector3 {
            std::cos(angle) * 0.55f, std::sin(angle) * 0.55f, -1.0f });
        auto bullet = std::make_unique<NormalEnemyBullet>();
        bullet->Initialize(bulletModel_);
        bullet->SetTranslate(muzzle);
        bullet->SetVelocity(direction * speed);
        bullet->SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
        bullet->SetScale({ 0.4f, 0.4f, 0.9f });
        bullet->SetDamage(damage);
        AddEnemyBullet(std::move(bullet));
    }
}

void IceJellyfish::FirePulseRing(
    int count, float spread, float speed, float angleOffset, bool includeCenter)
{
    if (player_ == nullptr || bulletModel_ == nullptr || count <= 0) return;
    const Vector3 muzzle = bodyPosition_ + Vector3 { 0.0f, -2.0f, -15.0f };
    const Vector3 targetDirection = Normalize(player_->GetTranslate() - muzzle);
    int ringCount = count;

    if (includeCenter) {
        auto centerBullet = std::make_unique<NormalEnemyBullet>();
        centerBullet->Initialize(bulletModel_);
        centerBullet->SetTranslate(muzzle);
        centerBullet->SetVelocity(targetDirection * speed);
        centerBullet->SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
        centerBullet->SetScale({ 0.46f, 0.46f, 0.95f });
        centerBullet->SetDamage(1);
        AddEnemyBullet(std::move(centerBullet));
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
        bullet->Initialize(bulletModel_);
        bullet->SetTranslate(muzzle);
        bullet->SetVelocity(direction * speed);
        bullet->SetColor({ 1.0f, 0.353f, 0.239f, 1.0f });
        bullet->SetScale({ 0.42f, 0.42f, 0.90f });
        bullet->SetDamage(1);
        AddEnemyBullet(std::move(bullet));
    }
}

bool IceJellyfish::IsSegmentAlive(size_t tentacle, size_t segment) const
{
    const float segmentThreshold = kSegmentHp * static_cast<float>(segment);
    return tentacleHp_[tentacle] > segmentThreshold;
}

bool IceJellyfish::AreAllTentaclesDestroyed() const
{
    return std::all_of(tentacleHp_.begin(), tentacleHp_.end(),
        [](float hp) { return hp <= 0.0f; });
}

bool IceJellyfish::IsIceSpearEmitter(size_t tentacle, size_t segment) const
{
    if (iceSpearVolley_ < 0 || !IsSegmentAlive(tentacle, segment)) return false;
    if (!IsIceSpearTentacleSelected(tentacle)) return false;
    return segment + 1 == kSegmentsPerTentacle || !IsSegmentAlive(tentacle, segment + 1);
}

bool IceJellyfish::IsIceSpearTentacleSelected(size_t tentacle) const
{
    if (iceSpearVolley_ < 0) return false;
    for (int32_t selectedTentacle : iceSpearTentacles_) {
        if (selectedTentacle == static_cast<int32_t>(tentacle)) {
            return true;
        }
    }
    return false;
}

Vector3 IceJellyfish::GetIceSpearAimTarget(size_t tentacle) const
{
    constexpr std::array<Vector3, 3> targetOffsets = {
        Vector3 { -14.0f, 0.0f, 0.0f },
        Vector3 { 14.0f, 0.0f, 0.0f },
        Vector3 { 0.0f, 0.0f, 0.0f }
    };
    for (size_t index = 0; index < iceSpearTentacles_.size(); ++index) {
        if (iceSpearTentacles_[index] == static_cast<int32_t>(tentacle)) {
            return lockedTarget_ + targetOffsets[index];
        }
    }
    return lockedTarget_;
}

int32_t IceJellyfish::SegmentPartIndex(size_t tentacle, size_t segment) const
{
    return kFirstSegmentPart + static_cast<int32_t>(tentacle * kSegmentsPerTentacle + segment);
}

void IceJellyfish::UpdatePartTransforms()
{
    if (bell_ == nullptr) return;

    const float floatPhase = animationTime_ * 1.10f;
    const float contractionAmount = std::clamp(-std::sin(floatPhase), 0.0f, 1.0f);
    const Vector3 rotation = {};
    const Matrix4x4 bodyMatrix = MatrixMath::MakeAffineMatrix(
        { 1.0f, 1.0f, 1.0f }, rotation, bodyPosition_);
    const float pulse = 0.0f;
    const float openLift = 0.0f;
    const Matrix4x4 bellLocal = MatrixMath::MakeAffineMatrix(
        Vector3 {
            1.0f - contractionAmount * 0.08f,
            1.0f - contractionAmount * 0.14f,
            1.0f - contractionAmount * 0.08f },
        Vector3 {}, Vector3 { 0.0f, openLift, 0.0f });
    bell_->SetCustomWorldMatrix(MatrixMath::Multiply(bellLocal, bodyMatrix));
    bell_->SetColor({ 0.65f, 0.86f, 1.0f, 1.0f });
    bell_->Update();

    const float coreScale = 1.0f - contractionAmount * 0.08f;
    const Matrix4x4 coreLocal = MatrixMath::MakeAffineMatrix(
        Vector3 { coreScale, coreScale, coreScale },
        Vector3 {}, Vector3 { 0.0f, -2.0f, 0.0f });
    core_->SetCustomWorldMatrix(MatrixMath::Multiply(coreLocal, bodyMatrix));
    core_->SetColor(hitFlashTime_ > 0.0f
        ? Vector4 { 4.0f, 3.4f, 2.8f, 1.0f }
        : (coreExposed_ ? Vector4 { 3.6f, 1.8f, 0.35f, 1.0f }
                        : Vector4 { 2.8f, 1.25f, 0.25f, 1.0f }));
    core_->Update();
    coreCollider_ = { MatrixMath::Transform({}, core_->GetWorldMatrix()), 3.6f * coreScale };

    IceJellyfishCollision::BuildBell(bodyMatrix, pulse, bellColliders_);
    tentacleColliders_.clear();
    tentacleColliderParts_.clear();

    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        const float azimuth = kTwoPi * static_cast<float>(tentacle) /
            static_cast<float>(kTentacleCount) + 0.25f;
        const bool icePillarTentacle = icePillarWave_ >= 0 &&
            static_cast<int32_t>(tentacle % 2) == icePillarWave_ % 2;
        const float icePillarCharge = icePillarTentacle
            ? std::clamp(icePillarTimer_ / kIcePillarRevealSeconds, 0.0f, 1.0f)
            : 0.0f;
        const float tentaclePhase =
            floatPhase * tentacleWaveSpeed_[tentacle] + tentacleWavePhase_[tentacle];
        const float tentacleAmplitude = tentacleWaveAmplitude_[tentacle];
        const float radialX = std::cos(azimuth);
        const float radialZ = std::sin(azimuth);
        Vector3 joint = { radialX * 10.5f, -1.2f, radialZ * 10.5f };
        const float contraction = std::sin(tentaclePhase * 0.72f);
        float bend = 0.30f + contraction * 0.34f * tentacleAmplitude -
            contractionAmount * 0.42f;

        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            const float index = static_cast<float>(segment);
            const float tipWeight = index / static_cast<float>(kSegmentsPerTentacle - 1);
            const float tipInfluence = tipWeight * tipWeight;
            const float waveDelay = index * 0.52f + index * index * 0.035f;
            const float delayedPhase = tentaclePhase * 1.15f - waveDelay;
            const float tipCurl = std::sin(tentaclePhase * 1.30f - index * 0.90f) *
                tipInfluence * 0.24f * tentacleAmplitude;
            bend += 0.045f + std::sin(delayedPhase + azimuth * 0.35f) *
                (0.070f + index * 0.012f) * tentacleAmplitude + tipCurl -
                contractionAmount * (0.025f + index * 0.010f);
            const float sway = std::sin(tentaclePhase * 0.95f - index * 0.58f + azimuth) *
                (0.025f + index * 0.011f + tipInfluence * 0.10f) * tentacleAmplitude;
            Vector3 down = {
                radialX * std::sin(bend) - radialZ * sway,
                -std::cos(bend),
                radialZ * std::sin(bend) + radialX * sway
            };

            const bool iceSpearTentacle = IsIceSpearTentacleSelected(tentacle);
            if (iceSpearTentacle && IsSegmentAlive(tentacle, segment) && player_ != nullptr) {
                Vector3 aimTarget = GetIceSpearAimTarget(tentacle);
                aimTarget.z = player_->GetTranslate().z;
                const Vector3 aimDirection = Normalize((aimTarget - bodyPosition_) - joint);
                const float aimWeight = (0.15f + tipWeight * 0.67f) * iceSpearPoseWeight_;
                down = Normalize(down * (1.0f - aimWeight) + aimDirection * aimWeight);
            }
            if (icePillarTentacle && IsSegmentAlive(tentacle, segment)) {
                const Vector3 floorDirection = Normalize(Vector3 {
                    radialX * 0.08f,
                    -1.0f,
                    radialZ * 0.08f });
                const float pointDownWeight = icePillarCharge * (0.35f + tipWeight * 0.25f);
                down = Normalize(
                    down * (1.0f - pointDownWeight) + floorDirection * pointDownWeight);
            }

            down = Normalize(down);
            const Vector3 up = down * -1.0f;
            const Vector3 tangent = { -radialZ, 0.0f, radialX };
            Vector3 right = Normalize(Cross(up, tangent));
            const Vector3 forward = Cross(right, up);
            float length = (5.4f - index * 0.32f) * 1.5f;
            const float width = 6.6f - index * 1.0f;
            Matrix4x4 local = MatrixMath::MakeIdentity4x4();
            local.m[0][0] = right.x * width;
            local.m[0][1] = right.y * width;
            local.m[0][2] = right.z * width;
            local.m[1][0] = up.x * length;
            local.m[1][1] = up.y * length;
            local.m[1][2] = up.z * length;
            local.m[2][0] = forward.x * width;
            local.m[2][1] = forward.y * width;
            local.m[2][2] = forward.z * width;
            local.m[3][0] = joint.x;
            local.m[3][1] = joint.y;
            local.m[3][2] = joint.z;
            Object3d& object = *tentacles_[tentacle][segment];
            object.SetCustomWorldMatrix(MatrixMath::Multiply(local, bodyMatrix));
            const bool iceSpearEmitter = IsIceSpearEmitter(tentacle, segment);
            const bool iceSpearChargedSegment = iceSpearEmitter ||
                (segment + 1 < kSegmentsPerTentacle &&
                    IsIceSpearEmitter(tentacle, segment + 1));
            object.SetColor(iceSpearChargedSegment
                ? Vector4 {
                    0.55f + iceSpearPoseWeight_ * 1.10f,
                    0.80f - iceSpearPoseWeight_ * 0.35f,
                    1.00f + iceSpearPoseWeight_ * 1.80f,
                    1.0f }
                : (icePillarTentacle
                    ? Vector4 {
                        0.55f + icePillarCharge * 0.65f,
                        0.80f + icePillarCharge * 1.35f,
                        1.00f + icePillarCharge * 1.70f,
                        1.0f }
                    : Vector4 { 0.55f, 0.80f, 1.0f, 1.0f }));
            object.Update();
            if (IsSegmentAlive(tentacle, segment)) {
                tentacleColliders_.push_back(IceJellyfishCollision::TransformBox(
                    object.GetWorldMatrix(), { 0.0f, -0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }));
                tentacleColliderParts_.push_back(SegmentPartIndex(tentacle, segment));
            }
            joint = joint + down * (length + 0.18f);
        }
    }
    UpdateCrystalTransforms();
    UpdateIcePillarModelTransforms();
}

void IceJellyfish::UpdateCrystalTransforms()
{
    constexpr std::array<Vector3, 4> offsets = {
        Vector3 { -16.0f, -11.0f, 42.0f }, Vector3 { 16.0f, -11.0f, 42.0f },
        Vector3 { -16.0f, 11.0f, 42.0f }, Vector3 { 16.0f, 11.0f, 42.0f }
    };
    for (size_t index = 0; index < crystals_.size(); ++index) {
        if (crystalHp_[index] <= 0.0f || attackPattern_ != AttackPattern::CrystalPrison) continue;
        const Vector3 position = lockedTarget_ + offsets[index];
        const Matrix4x4 matrix = MatrixMath::MakeAffineMatrix(
            Vector3 { 2.2f, 5.0f, 2.2f },
            Vector3 { 0.0f, animationTime_ * 0.5f, 0.0f }, position);
        crystals_[index]->SetCustomWorldMatrix(matrix);
        crystals_[index]->Update();
        crystalColliders_[index] = { position, 4.0f };
    }
}

void IceJellyfish::UpdateIcePillarModelTransforms()
{
    if (!icePillarModelSpawnedForWave_) return;

    const float scaleWeight = (std::max)(0.12f,
        std::clamp(icePillarModelTimer_ / 0.18f, 0.0f, 1.0f));
    for (size_t index = 0; index < icePillarModelPositions_.size(); ++index) {
        Vector3 position = icePillarModelPositions_[index];
        position.y += 0.35f;
        const float heightVariation = 0.88f + static_cast<float>(index % 3) * 0.08f;
        const Matrix4x4 matrix = MatrixMath::MakeAffineMatrix(
            Vector3 { 4.0f * scaleWeight, 11.0f * heightVariation * scaleWeight,
                4.0f * scaleWeight },
            Vector3 { 0.0f, (static_cast<float>(index) - 2.5f) * 0.08f, 0.0f },
            position);
        icePillarModels_[index]->SetCustomWorldMatrix(matrix);
        icePillarModels_[index]->Update();
    }
}

void IceJellyfish::Draw()
{
    if (bell_ == nullptr || (isDead_ && deathTimer_ >= 2.0f)) return;
    for (size_t tentacle = 0; tentacle < kTentacleCount; ++tentacle) {
        for (size_t segment = 0; segment < kSegmentsPerTentacle; ++segment) {
            if (IsSegmentAlive(tentacle, segment)) tentacles_[tentacle][segment]->Draw();
        }
    }
    for (size_t crystal = 0; crystal < crystals_.size(); ++crystal) {
        if (attackPattern_ == AttackPattern::CrystalPrison && crystalHp_[crystal] > 0.0f) {
            crystals_[crystal]->Draw();
        }
    }
    if (icePillarModelSpawnedForWave_) {
        for (const std::unique_ptr<Object3d>& pillar : icePillarModels_) pillar->Draw();
    }
    core_->Draw();
    bell_->Draw();
}

SweepHit IceJellyfish::SweepBullet(
    const Sphere& bullet, const Vector3& movement, int32_t& partIndex) const
{
    SweepHit nearest {};
    partIndex = kBellPart;
    if (bell_ == nullptr || isDead_) return nearest;

    auto consider = [&](const SweepHit& hit, int32_t candidatePart) {
        if (hit.isHit && (!nearest.isHit || hit.time < nearest.time)) {
            nearest = hit;
            partIndex = candidatePart;
        }
    };
    if (!coreExposed_) {
        for (const OBB& box : bellColliders_) {
            consider(CollisionManager::SweepSphere(bullet, movement, box), kBellPart);
        }
    }
    for (size_t index = 0; index < tentacleColliders_.size(); ++index) {
        consider(CollisionManager::SweepSphere(bullet, movement, tentacleColliders_[index]),
            tentacleColliderParts_[index]);
    }
    if (attackPattern_ == AttackPattern::CrystalPrison) {
        for (size_t crystal = 0; crystal < crystals_.size(); ++crystal) {
            if (crystalHp_[crystal] > 0.0f) {
                consider(CollisionManager::SweepSphere(bullet, movement, crystalColliders_[crystal]),
                    kFirstCrystalPart + static_cast<int32_t>(crystal));
            }
        }
    }
    consider(CollisionManager::SweepSphere(bullet, movement, coreCollider_), kCorePart);
    return nearest;
}

void IceJellyfish::OnBulletHit(int32_t partIndex, float damage, const Vector3& position)
{
    if (IsCollisionPartDamageable(partIndex)) {
        ApplyDamageToPart(partIndex, damage);
        EffectManager::GetInstance()->PlayEffect("HitEffect", position);
    } else {
        OnCollisionPartGuarded(partIndex, position);
    }
}

bool IceJellyfish::IsCollisionPartDamageable(int32_t partIndex) const
{
    if (partIndex == kCorePart) return coreExposed_;
    if (partIndex >= kFirstSegmentPart &&
        partIndex < kFirstSegmentPart + static_cast<int32_t>(kTentacleCount * kSegmentsPerTentacle)) {
        const int32_t flat = partIndex - kFirstSegmentPart;
        return IsSegmentAlive(flat / static_cast<int32_t>(kSegmentsPerTentacle),
            flat % static_cast<int32_t>(kSegmentsPerTentacle));
    }
    if (partIndex >= kFirstCrystalPart && partIndex < kFirstCrystalPart + 4) {
        return crystalHp_[partIndex - kFirstCrystalPart] > 0.0f;
    }
    return false;
}

void IceJellyfish::ApplyDamageToPart(int32_t partIndex, float damage)
{
    if (isDead_ || damage <= 0.0f) return;
    if (partIndex == kCorePart) {
        if (!coreExposed_) return;
        hitFlashTime_ = 0.12f;
        hp_ = (std::max)(0.0f, hp_ - damage * 1.5f);
    } else if (partIndex >= kFirstSegmentPart &&
        partIndex < kFirstSegmentPart + static_cast<int32_t>(kTentacleCount * kSegmentsPerTentacle)) {
        const int32_t flat = partIndex - kFirstSegmentPart;
        const size_t tentacle = static_cast<size_t>(flat) / kSegmentsPerTentacle;
        const float before = tentacleHp_[tentacle];
        tentacleHp_[tentacle] = (std::max)(0.0f, before - damage);
        for (size_t remaining = kSegmentsPerTentacle; remaining > 0; --remaining) {
            const size_t segment = remaining - 1;
            const bool wasAlive = before > kSegmentHp * static_cast<float>(segment);
            if (wasAlive && !IsSegmentAlive(tentacle, segment)) {
                const Vector3 burstPosition = MatrixMath::Transform(
                    {}, tentacles_[tentacle][segment]->GetWorldMatrix());
                EffectManager::GetInstance()->PlayEffect("Explosion", burstPosition);
            }
        }
        if (attackPattern_ == AttackPattern::AbsoluteZero) {
            absoluteSealDamage_[tentacle] += damage;
        }
        coreExposed_ = AreAllTentaclesDestroyed();
    } else if (partIndex >= kFirstCrystalPart && partIndex < kFirstCrystalPart + 4) {
        const size_t crystal = static_cast<size_t>(partIndex - kFirstCrystalPart);
        crystalHp_[crystal] = (std::max)(0.0f, crystalHp_[crystal] - damage);
    } else {
        return;
    }

    if (hp_ <= 0.0f) {
        SetDead(true);
        EffectManager::GetInstance()->PlayEffect("Explosion", coreCollider_.center);
    }
}

void IceJellyfish::OnCollisionPartGuarded(int32_t, const Vector3& position)
{
    EffectManager::GetInstance()->PlayEffect("HitEffect", position);
}

void IceJellyfish::GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const
{
    parts.clear();
    parts.push_back({ coreCollider_.center, coreCollider_.radius, kCorePart });
    for (size_t index = 0; index < tentacleColliders_.size(); ++index) {
        const OBB& box = tentacleColliders_[index];
        const float radius = (std::max)(box.size.x, (std::max)(box.size.y, box.size.z));
        parts.push_back({ box.center, radius, tentacleColliderParts_[index] });
    }
}

void IceJellyfish::RegisterRaycastTargets(CollisionObjectId& nextObjectId) const
{
    if (bell_ == nullptr || isDead_) return;
    CollisionManager* manager = CollisionManager::GetInstance();
    if (!coreExposed_) {
        for (const OBB& box : bellColliders_) manager->RegisterRaycastObbTarget(nextObjectId++, box);
    }
    for (const OBB& box : tentacleColliders_) manager->RegisterRaycastObbTarget(nextObjectId++, box);
    for (size_t crystal = 0; crystal < crystals_.size(); ++crystal) {
        if (attackPattern_ == AttackPattern::CrystalPrison && crystalHp_[crystal] > 0.0f) {
            manager->RegisterRaycastSphereTarget(nextObjectId++, crystalColliders_[crystal]);
        }
    }
    manager->RegisterRaycastSphereTarget(nextObjectId++, coreCollider_);
}

void IceJellyfish::DrawCollisionDebug() const
{
    if (bell_ == nullptr || isDead_) return;
    DebugRenderer* renderer = DebugRenderer::GetInstance();
    if (!coreExposed_) {
        for (const OBB& box : bellColliders_) {
            renderer->AddWireOBB(box.center, box.size, box.orientation[0], box.orientation[1],
                box.orientation[2], { 0.1f, 0.7f, 1.0f, 1.0f });
        }
    }
    for (const OBB& box : tentacleColliders_) {
        renderer->AddWireOBB(box.center, box.size, box.orientation[0], box.orientation[1],
            box.orientation[2], { 0.8f, 0.3f, 1.0f, 1.0f });
    }
    if (icePillarModelSpawnedForWave_) {
        constexpr Vector3 axisX = { 1.0f, 0.0f, 0.0f };
        constexpr Vector3 axisY = { 0.0f, 1.0f, 0.0f };
        constexpr Vector3 axisZ = { 0.0f, 0.0f, 1.0f };
        constexpr Vector3 collisionSize = { 13.0f, 42.0f, 8.0f };
        for (const Vector3& position : icePillarPositions_) {
            const Vector3 collisionCenter = {
                position.x,
                position.y + 19.0f,
                position.z
            };
            renderer->AddWireOBB(collisionCenter, collisionSize,
                axisX, axisY, axisZ, { 1.0f, 0.15f, 0.05f, 1.0f }, 3.0f);
        }
    }
    renderer->AddWireSphere(coreCollider_.center, coreCollider_.radius,
        { 1.0f, 0.6f, 0.1f, 1.0f }, 2.0f);
}

float IceJellyfish::GetHeadHpFraction() const
{
    return std::clamp(hp_ / kMaxHp, 0.0f, 1.0f);
}

float IceJellyfish::GetBodyHpFraction() const
{
    float remaining = 0.0f;
    for (float tentacle : tentacleHp_) remaining += tentacle;
    return remaining / (kTentacleMaxHp * static_cast<float>(kTentacleCount));
}
