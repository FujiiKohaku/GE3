#pragma once

#include "../State/FearWormBattleAction.h"
#include "FearWormBeam.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormBeamState final : public FearWormBattleAction {
public:
    enum class Phase { Wait, Charge, Fire, FadeOut };
    void Initialize();
    void Reset();
    void Stop();
    bool IsAiming() const { return phase_ != Phase::Wait; }
    bool ShouldAimHead(const FearWormEnemy& boss) const override;
    bool IsHittingPlayer() const { return beam_.IsHittingPlayer(); }
    void Draw();
    void SubmitRaytracing(DxrRenderer& renderer);
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    friend bool RunFearWormStateTests();
    friend bool RunBeamStateTests();
    friend bool RunFearWormBattleActionTests();
    void UpdateMovement(FearWormBattleState& battle, FearWormEnemy& boss);
    void UpdatePhase(FearWormBattleState& battle, FearWormEnemy& boss, float deltaTime);
    void UpdateWait(FearWormBattleState& battle, FearWormEnemy& boss, float deltaTime);
    void UpdateCharge(FearWormBattleState& battle, FearWormEnemy& boss, float deltaTime);
    void UpdateFire(FearWormBattleState& battle, FearWormEnemy& boss, float deltaTime);
    void UpdateFadeOut(FearWormBattleState& battle, FearWormEnemy& boss, float deltaTime);
    void ChangePhase(FearWormBattleState& battle, FearWormEnemy& boss, Phase phase);
    FearWormBeam beam_;
    int32_t pendingShotEffectCount_ = 0;
    Phase phase_ = Phase::Wait;
    float elapsedSeconds_ = 0.0f;
    float effectTimerSeconds_ = 0.0f;
};
