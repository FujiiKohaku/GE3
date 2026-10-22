#pragma once

#include "../State/FearWormBattleAction.h"
#include "../Movement/FearWormMovement.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormWeaveSpreadState final : public FearWormBattleAction {
public:
    static Vector3 CalculateTarget(const FearWormMovementContext& context);
    void Reset();
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    float elapsedSeconds_ = 0.0f;
    static constexpr float kWeaveAngleRate = 1.35f;
    static constexpr float kWeaveWidth = 13.0f;
    static constexpr float kWeaveHeight = 7.0f;
    static constexpr float kWeaveVerticalFrequency = 2.0f;
    static constexpr float kWeaveVerticalAmplitude = 9.0f;
    static constexpr float kWeaveDepthAmplitude = 2.5f;
    static constexpr float kNormalSpreadAngleRadians = 0.14f;

    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void FireSpreadBullets(FearWormBattleState& battle, FearWormEnemy& boss,
        const Vector3& position, const Vector3& direction);
    float fireElapsedSeconds_ = 0.0f;
};
