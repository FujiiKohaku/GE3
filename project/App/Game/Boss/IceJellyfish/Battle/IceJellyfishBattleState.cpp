#include "IceJellyfishBattleState.h"
#include "../IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include <cmath>

namespace {
// 調整用の設定値。意味が異なる値は、数値が同じでも個別に保持する。
constexpr float kHoverHorizontalFrequency = 0.48f;
constexpr float kHoverHorizontalAmplitude = 8.0f;
constexpr float kHoverVerticalFrequency = 1.10f;
constexpr float kHoverVerticalAmplitude = 5.0f;
constexpr float kAttackArmThreshold = -0.72f;
constexpr float kAttackStartThreshold = -0.05f;
}


void IceJellyfishBattleState::Reset()
{
    nextAttackIndex_ = 0;
    isPulseAttackArmed_ = false;
    pulseBarrageAttack_.Reset();
    iceSpearAttack_.Reset();
    icePillarAttack_.Reset();
}

void IceJellyfishBattleState::Exit(IceJellyfish&)
{
    Reset();
}

void IceJellyfishBattleState::Update(IceJellyfish& boss)
{
    if (boss.player_ != nullptr) {
        const Vector3 playerPosition = boss.player_->GetTranslate();
        boss.bodyPosition_ = {
            boss.hoverCenterX_ + std::sin(boss.animationTime_ * kHoverHorizontalFrequency) * kHoverHorizontalAmplitude,
            boss.hoverCenterY_ + std::sin(boss.animationTime_ * kHoverVerticalFrequency) * kHoverVerticalAmplitude,
            playerPosition.z + boss.kHoverDistance
        };
    } else {
        boss.bodyPosition_ = boss.basePosition_;
    }

    boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
    UpdateAttackSequence(boss, boss.animationTime_ * kHoverVerticalFrequency, TimeManager::GetInstance()->GetDeltaTime());
    boss.coreExposed_ = boss.AreAllTentaclesDestroyed();
}

void IceJellyfishBattleState::UpdateAttackSequence(IceJellyfish& boss, float floatPhase, float deltaTimeSeconds)
{
    // 前の攻撃が継続中でも更新し、新たな攻撃の開始と両立させる。
    pulseBarrageAttack_.Update(boss, deltaTimeSeconds);
    iceSpearAttack_.Update(boss, deltaTimeSeconds);
    icePillarAttack_.Update(boss, deltaTimeSeconds);
    const float pulse = std::sin(floatPhase);
    if (pulse <= kAttackArmThreshold) {
        isPulseAttackArmed_ = true;
    }
    if (!isPulseAttackArmed_ || pulse < kAttackStartThreshold) {
        return;
    }

    switch (nextAttackIndex_) {
    case 0: pulseBarrageAttack_.Start(); break;
    case 1: iceSpearAttack_.Start(boss); break;
    case 2: icePillarAttack_.Start(boss); break;
    }
    nextAttackIndex_ = (nextAttackIndex_ + 1) % 3;
    isPulseAttackArmed_ = false;
}

bool IceJellyfishBattleState::IsIceSpearEmitter(const IceJellyfish& boss, size_t tentacle, size_t segment) const
{
    return iceSpearAttack_.IsEmitter(boss, tentacle, segment);
}

bool IceJellyfishBattleState::IsIceSpearTentacleSelected(size_t tentacle) const
{
    return iceSpearAttack_.IsTentacleSelected(tentacle);
}

Vector3 IceJellyfishBattleState::GetIceSpearAimTarget(size_t tentacle) const
{
    return iceSpearAttack_.GetAimTarget(tentacle);
}

bool IceJellyfishBattleState::IsIcePillarTentacle(size_t tentacle) const
{
    return icePillarAttack_.IsTentacleSelected(tentacle);
}

float IceJellyfishBattleState::GetIcePillarCharge() const
{
    return icePillarAttack_.GetCharge();
}

float IceJellyfishBattleState::GetIcePillarModelScaleWeight() const
{
    return icePillarAttack_.GetModelScaleWeight();
}

Vector3 IceJellyfishBattleState::ApplyAttackTentacleDirection(const IceJellyfish& boss,
    const IceJellyfishAttackMotionContext& context, const Vector3& direction) const
{
    const Vector3 spearDirection = iceSpearAttack_.ApplyTentacleDirection(boss, context, direction);
    return icePillarAttack_.ApplyTentacleDirection(boss, context, spearDirection);
}

Vector4 IceJellyfishBattleState::ApplyAttackTentacleColor(const IceJellyfish& boss,
    const IceJellyfishAttackMotionContext& context, const Vector4& baseColor) const
{
    Vector4 color = baseColor;
    if (iceSpearAttack_.TryApplyTentacleColor(boss, context, baseColor, color)) {
        return color;
    }
    return icePillarAttack_.ApplyTentacleColor(context, baseColor);
}
