#include "FearWormBeamState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include <algorithm>
#include <cmath>
#include "../State/FearWormTiming.h"

void FearWormBeamState::Initialize()
{
    beam_.Initialize();
    Reset();
}

void FearWormBeamState::Reset()
{
    isFinished_ = false;
    phase_ = Phase::Wait;
    elapsedSeconds_ = 0.0f;
    effectTimerSeconds_ = 0.0f;
    pendingShotEffectCount_ = 0;
    hasCompletedIntimidation_ = false;
    didStartIntimidationThisFrame_ = false;
    beam_.Reset();
}

void FearWormBeamState::Enter(FearWormBattleState& battle, FearWormEnemy& boss)
{
    Stop();
    isFinished_ = false;
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Drift;
    if (!boss.HasAttackBodyParts() && !hasCompletedIntimidation_) {
        centeringOffset_ = boss.GetPosition() - CalculateIntimidationCenter(battle, boss);
        ChangePhase(battle, boss, Phase::Centering);
    }
}

void FearWormBeamState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    elapsedSeconds_ += deltaTimeSeconds;
    didStartIntimidationThisFrame_ = false;
    UpdateMovement(battle, boss);
    pendingShotEffectCount_ = 0;
    UpdatePhase(battle, boss, deltaTimeSeconds);
    // 照準と段階の更新後に姿勢を確定し、描画・判定・射撃演出で共有する。
    beam_.SetPose(battle.HeadMuzzlePosition(boss), battle.headAimDirection_);
    if (phase_ == Phase::Fire) {
        boss.CheckAttackBeamCollision(beam_);
    } else {
        beam_.ClearHitState();
    }
    for (int32_t effectIndex = 0; effectIndex < pendingShotEffectCount_; effectIndex++) {
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", beam_.GetPosition());
    }
    beam_.UpdateVisual(deltaTimeSeconds);
}

Vector3 FearWormBeamState::CalculateIntimidationCenter(const FearWormBattleState& battle, const FearWormEnemy& boss) const
{
    constexpr float kCenterHeightOffset = 8.0f;
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.GetAttackTargetPosition());
    return context.playerPosition + Vector3 { 0.0f, kCenterHeightOffset, context.forwardOffset };
}

void FearWormBeamState::UpdateMovement(FearWormBattleState& battle, FearWormEnemy& boss)
{
    if (phase_ == Phase::Centering || phase_ == Phase::Intimidating) {
        Vector3 target = CalculateIntimidationCenter(battle, boss);
        if (phase_ == Phase::Centering) {
            const float progress = std::clamp(elapsedSeconds_ / kCenteringDurationSeconds, 0.0f, 1.0f);
            const float blend = progress * progress * (3.0f - 2.0f * progress);
            target = target + centeringOffset_ * (1.0f - blend);
        }
        battle.SetHeadPosition(boss, target);
        return;
    }

    const float movementSpeedRate = boss.GetAttackMovementSpeedRate();
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.GetAttackTargetPosition());
    const Vector3 target = FearWormMovement::CalculateDriftTarget(context);
    if (!IsAiming()) {
        battle.UpdateBattleMovement(boss, target, movementSpeedRate);
        return;
    }

    // チャージから消失まではX・Yを固定し、Z方向の追従だけを維持する。
    boss.FollowAttackDepth(target.z, movementSpeedRate);
}

void FearWormBeamState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    Stop();
}

bool FearWormBeamState::ShouldAimHead(const FearWormEnemy& boss) const
{
    return !boss.HasAttackBodyParts() && IsAiming();
}

void FearWormBeamState::Stop()
{
    phase_ = Phase::Wait;
    elapsedSeconds_ = 0.0f;
    effectTimerSeconds_ = 0.0f;
    pendingShotEffectCount_ = 0;
    didStartIntimidationThisFrame_ = false;
    beam_.Stop();
}

void FearWormBeamState::Draw()
{
    if (IsAiming()) {
        beam_.Draw();
    }
}

void FearWormBeamState::SubmitRaytracing(DxrRenderer& renderer)
{
    if (IsAiming()) {
        beam_.SubmitRaytracing(renderer);
    }
}

void FearWormBeamState::UpdatePhase(FearWormBattleState& battle, FearWormEnemy& boss, float deltaTime)
{
    switch (phase_) {
    case Phase::Centering:
        if (elapsedSeconds_ >= kCenteringDurationSeconds) {
            ChangePhase(battle, boss, Phase::Intimidating);
        }
        break;
    case Phase::Intimidating:
        if (elapsedSeconds_ >= kIntimidationDurationSeconds) {
            hasCompletedIntimidation_ = true;
            ChangePhase(battle, boss, Phase::Charge);
        }
        break;
    case Phase::Wait: UpdateWait(battle, boss, deltaTime); break;
    case Phase::Charge: UpdateCharge(battle, boss, deltaTime); break;
    case Phase::Fire: UpdateFire(battle, boss, deltaTime); break;
    case Phase::FadeOut: UpdateFadeOut(battle, boss, deltaTime); break;
    }
}

void FearWormBeamState::UpdateWait(FearWormBattleState& battle, FearWormEnemy& boss, float)
{
    beam_.SetDimensions(0.0f, 0.0f);
    if (elapsedSeconds_ >= 4.0f) ChangePhase(battle, boss, Phase::Charge);
}

void FearWormBeamState::UpdateCharge(FearWormBattleState& battle, FearWormEnemy& boss, float dt)
{
    beam_.SetDimensions(0.0f, 0.0f);
    const Vector3 direction = NormalizeSafe(boss.GetAttackTargetPosition() - boss.GetAttackSegmentPosition(0));
    battle.headAimDirection_ = NormalizeSafe(Lerp(battle.headAimDirection_, direction,
        FearWormTiming::CalculateFollowBlend(0.08f, dt)));
    effectTimerSeconds_ += dt;
    if (effectTimerSeconds_ >= 0.25f) {
        effectTimerSeconds_ = std::fmod(effectTimerSeconds_, 0.25f);
        pendingShotEffectCount_++;
    }
    if (elapsedSeconds_ >= 1.5f) ChangePhase(battle, boss, Phase::Fire);
}

void FearWormBeamState::UpdateFire(FearWormBattleState& battle, FearWormEnemy& boss, float dt)
{
    beam_.SetDimensions((std::min)(400.0f, beam_.GetLength() + 1000.0f * dt), 3.5f);
    const Vector3 direction = NormalizeSafe(boss.GetAttackTargetPosition() - boss.GetAttackSegmentPosition(0));
    battle.headAimDirection_ = NormalizeSafe(Lerp(battle.headAimDirection_, direction,
        FearWormTiming::CalculateFollowBlend(0.015f, dt)));
    effectTimerSeconds_ += dt;
    if (effectTimerSeconds_ >= 0.2f) {
        effectTimerSeconds_ = std::fmod(effectTimerSeconds_, 0.2f);
        pendingShotEffectCount_++;
    }
    if (elapsedSeconds_ >= 3.0f) ChangePhase(battle, boss, Phase::FadeOut);
}

void FearWormBeamState::UpdateFadeOut(FearWormBattleState& battle, FearWormEnemy& boss, float dt)
{
    beam_.SetDimensions(beam_.GetLength(), (std::max)(0.0f, beam_.GetWidth() - 8.0f * dt));
    const Vector3 direction = NormalizeSafe(boss.GetAttackTargetPosition() - boss.GetAttackSegmentPosition(0));
    battle.headAimDirection_ = NormalizeSafe(Lerp(battle.headAimDirection_, direction,
        FearWormTiming::CalculateFollowBlend(0.01f, dt)));
    if (elapsedSeconds_ >= 0.5f) ChangePhase(battle, boss, Phase::Wait);
}

void FearWormBeamState::ChangePhase(FearWormBattleState& battle, FearWormEnemy& boss, Phase state)
{
    if (phase_ == state) return;
    phase_ = state;
    elapsedSeconds_ = 0.0f;
    if (state != Phase::Fire) {
        beam_.ClearHitState();
    }
    switch (state) {
    case Phase::Intimidating:
        didStartIntimidationThisFrame_ = true;
        break;
    case Phase::Charge: {
        effectTimerSeconds_ = 0.0f;
        if (boss.HasAttackTarget() && !(boss.GetAttackSegmentCount() == 0)) {
            battle.headAimDirection_ = NormalizeSafe(boss.GetAttackTargetPosition() - boss.GetAttackSegmentPosition(0));
            pendingShotEffectCount_++;
        }
        break;
    }
    case Phase::Fire: {
        effectTimerSeconds_ = 0.0f;
        if (boss.HasAttackTarget() && !(boss.GetAttackSegmentCount() == 0)) {
            pendingShotEffectCount_++;
        }
        break;
    }
    default: break;
    }
}

