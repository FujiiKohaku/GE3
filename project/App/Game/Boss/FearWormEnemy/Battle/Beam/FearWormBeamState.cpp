#include "FearWormBeamState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include <algorithm>

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
    beam_.Reset();
}

void FearWormBeamState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    Stop();
    isFinished_ = false;
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Drift;
}

void FearWormBeamState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    battle.movementPatternTimer_ += deltaTimeSeconds * boss.MovementSpeedRate();
    UpdateMovement(battle, boss);
    elapsedSeconds_ += deltaTimeSeconds;
    const bool shouldCheckCollision = phase_ == Phase::Fire;
    pendingShotEffectCount_ = 0;
    UpdatePhase(battle, boss, deltaTimeSeconds);
    // 照準と段階の更新後に姿勢を確定し、描画・判定・射撃演出で共有する。
    beam_.SetPose(battle.HeadMuzzlePosition(boss), battle.headAimDirection_);
    if (shouldCheckCollision) {
        beam_.CheckCollision(boss.player_);
    }
    for (int32_t effectIndex = 0; effectIndex < pendingShotEffectCount_; effectIndex++) {
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", beam_.GetPosition());
    }
    beam_.UpdateVisual(deltaTimeSeconds);
}

void FearWormBeamState::UpdateMovement(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float movementSpeedRate = boss.MovementSpeedRate();
    if (!IsAiming()) {
        battle.UpdateBattleMovement(boss, boss.player_->GetTranslate(), movementSpeedRate);
        return;
    }

    // チャージから消失まではX・Yを固定し、Z方向の追従だけを維持する。
    const Vector3 target = battle.BattleTarget(boss, boss.player_->GetTranslate());
    const float movementBlend = boss.moveSpeed_ * movementSpeedRate;
    Vector3& headPosition = boss.segments_[0].position;
    headPosition.z += (target.z - headPosition.z) * movementBlend;
    boss.transform_.translate = headPosition;
}

void FearWormBeamState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    Stop();
}

void FearWormBeamState::Stop()
{
    phase_ = Phase::Wait;
    elapsedSeconds_ = 0.0f;
    effectTimerSeconds_ = 0.0f;
    pendingShotEffectCount_ = 0;
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
    const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
    battle.headAimDirection_ = NormalizeSafe(Lerp(battle.headAimDirection_, direction, 0.08f));
    effectTimerSeconds_ += dt;
    if (effectTimerSeconds_ >= 0.25f) {
        effectTimerSeconds_ = 0.0f;
        pendingShotEffectCount_++;
    }
    if (elapsedSeconds_ >= 1.5f) ChangePhase(battle, boss, Phase::Fire);
}

void FearWormBeamState::UpdateFire(FearWormBattleState& battle, FearWormEnemy& boss, float dt)
{
    beam_.SetDimensions((std::min)(400.0f, beam_.GetLength() + 1000.0f * dt), 3.5f);
    const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
    battle.headAimDirection_ = NormalizeSafe(Lerp(battle.headAimDirection_, direction, 0.015f));
    effectTimerSeconds_ += dt;
    if (effectTimerSeconds_ >= 0.2f) {
        effectTimerSeconds_ = 0.0f;
        pendingShotEffectCount_++;
    }
    if (elapsedSeconds_ >= 3.0f) ChangePhase(battle, boss, Phase::FadeOut);
}

void FearWormBeamState::UpdateFadeOut(FearWormBattleState& battle, FearWormEnemy& boss, float dt)
{
    beam_.SetDimensions(beam_.GetLength(), (std::max)(0.0f, beam_.GetWidth() - 8.0f * dt));
    const Vector3 direction = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
    battle.headAimDirection_ = NormalizeSafe(Lerp(battle.headAimDirection_, direction, 0.01f));
    if (elapsedSeconds_ >= 0.5f) ChangePhase(battle, boss, Phase::Wait);
}

void FearWormBeamState::ChangePhase(FearWormBattleState& battle, FearWormEnemy& boss, Phase state)
{
    if (phase_ == state) return;
    phase_ = state;
    elapsedSeconds_ = 0.0f;
    switch (state) {
    case Phase::Charge: {
        effectTimerSeconds_ = 0.0f;
        if (boss.player_ && !boss.segments_.empty()) {
            battle.headAimDirection_ = NormalizeSafe(boss.player_->GetTranslate() - boss.segments_[0].position);
            pendingShotEffectCount_++;
        }
        break;
    }
    case Phase::Fire: {
        effectTimerSeconds_ = 0.0f;
        if (boss.player_ && !boss.segments_.empty()) {
            pendingShotEffectCount_++;
        }
        break;
    }
    default: break;
    }
}

