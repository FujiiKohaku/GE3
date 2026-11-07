#include "FearWormDeadState.h"
#include "../FearWormEnemy.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <algorithm>
#include <cmath>

void FearWormDeadState::Enter(FearWormEnemy& boss)
{
    boss.battleState_.StopAttacksForDeath(boss);
    isDeathSequenceFinished_ = false;
    elapsedSeconds_ = 0.0f;
    hasPlayedPrelude_ = false;
    hasBurst_ = false;
    for (size_t index = 1; index < boss.segments_.size(); ++index) {
        boss.segments_[index].isAlive = false;
    }
    if (boss.segments_.empty()) {
        isDeathSequenceFinished_ = true;
        return;
    }
    origin_ = boss.segments_[0].position;
    if (boss.segments_[0].object) {
        rotationRadians_ = boss.segments_[0].object->GetRotate();
    }
}

void FearWormDeadState::Burst(FearWormEnemy& boss)
{
    hasBurst_ = true;
    boss.segments_[0].position = origin_;
    boss.segments_[0].isAlive = false;
    auto* effectManager = EffectManager::GetInstance();
    effectManager->PlayEffect("WormDeathExplosion", origin_);
    effectManager->PlayEffect("MissileExplosionRing", origin_);
    effectManager->PlayEffect("FearWormHeadDebris", origin_);
}

float FearWormDeadState::GetCameraShakeStrength() const
{
    if (isDeathSequenceFinished_ || elapsedSeconds_ < kFreezeDurationSeconds) {
        return 0.0f;
    }
    if (elapsedSeconds_ < kBurstStartSeconds) {
        const float charge = (elapsedSeconds_ - kFreezeDurationSeconds)
            / (kBurstStartSeconds - kFreezeDurationSeconds);
        return 0.0015f + charge * charge * 0.0045f;
    }
    const float burstElapsedSeconds = elapsedSeconds_ - kBurstStartSeconds;
    if (burstElapsedSeconds < kImpactShakeDurationSeconds) {
        const float impactProgress = burstElapsedSeconds / kImpactShakeDurationSeconds;
        return kImpactShakeStrength * (1.0f - impactProgress * 0.25f);
    }
    const float remaining = 1.0f - std::clamp(
        (burstElapsedSeconds - kImpactShakeDurationSeconds) / kShakeDecayDurationSeconds, 0.0f, 1.0f);
    return kAftershockShakeStrength * remaining * remaining;
}

void FearWormDeadState::Update(FearWormEnemy& boss)
{
    if (isDeathSequenceFinished_) {
        return;
    }
    elapsedSeconds_ += (std::max)(0.0f, TimeManager::GetInstance()->GetDeltaTime());
    if (!hasPlayedPrelude_ && elapsedSeconds_ >= kFreezeDurationSeconds) {
        hasPlayedPrelude_ = true;
        EffectManager::GetInstance()->PlayEffect("FearWormDeathPrelude", origin_);
    }
    if (!hasBurst_ && elapsedSeconds_ >= kBurstStartSeconds) {
        Burst(boss);
    }
    if (hasBurst_) {
        isDeathSequenceFinished_ = elapsedSeconds_ >= kDeathDurationSeconds;
        return;
    }

    auto& head = boss.segments_[0];
    if (!head.object) {
        return;
    }
    const float charge = std::clamp((elapsedSeconds_ - kFreezeDurationSeconds)
        / (kBurstStartSeconds - kFreezeDurationSeconds), 0.0f, 1.0f);
    const float tremble = charge * charge;
    head.position = origin_ + Vector3{
        std::sin(elapsedSeconds_ * 91.0f) * tremble * 0.22f,
        charge * 0.45f + std::sin(elapsedSeconds_ * 113.0f) * tremble * 0.12f,
        std::cos(elapsedSeconds_ * 79.0f) * tremble * 0.12f };
    head.object->SetTranslate(head.position);
    head.object->SetRotate(rotationRadians_ + Vector3{
        std::sin(elapsedSeconds_ * 63.0f) * tremble * 0.035f,
        0.0f, std::cos(elapsedSeconds_ * 71.0f) * tremble * 0.045f });
    const float scaleRate = 1.0f + tremble * 0.08f;
    head.object->SetScale(head.scale * scaleRate);
    const float glow = charge * (0.65f + 0.35f * std::sin(elapsedSeconds_ * 36.0f));
    head.object->SetColor({ 1.0f, 0.18f + glow * 0.82f, 0.04f + glow * 0.96f, 1.0f });
    head.object->Update();
    if (boss.headEye_) {
        boss.headEye_->SetColor({ 1.0f - charge, 1.0f - charge, 1.0f - charge, 1.0f });
    }
}
