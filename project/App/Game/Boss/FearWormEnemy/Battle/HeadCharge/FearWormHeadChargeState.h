#pragma once

#include "../State/FearWormBattleAction.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>

class FearWormHeadChargeState final : public FearWormBattleAction {
public:
    Vector4 HeadAttackColor(const Vector4& baseColor) const;
    float ApplyHeadChargePulse(float basePulse) const;
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    static constexpr float kHeadChargeDurationSeconds = 0.85f;
    static constexpr float kHeadChargeShotIntervalSeconds = 0.22f;
    static constexpr float kHeadChargeEffectIntervalSeconds = 0.14f;
    static constexpr float kChargedBulletSpeed = 1.55f;
    static constexpr float kChargedBulletScale = 0.88f;
    static constexpr float kChargedBulletCollisionRadius = 2.75f;
    static constexpr int32_t kHeadChargeShotMax = 3;
    static constexpr int32_t kChargedBulletDamage = 3;
    static constexpr float kHeadAimFollowRate = 0.18f;
    static constexpr Vector4 kChargedBulletColor = { 1.0f, 0.353f, 0.239f, 1.0f };
    static constexpr float kChargedBulletLifeTimeSeconds = 20.0f;
    static constexpr Vector4 kChargingHeadColor = { 1.0f, 0.48f, 0.08f, 1.0f };
    static constexpr Vector4 kFiringHeadColor = { 1.0f, 0.08f, 0.02f, 1.0f };
    static constexpr float kChargePulseAdd = 0.12f;
    static constexpr float kChargePulseFrequency = 24.0f;
    static constexpr float kChargePulseAmplitude = 0.08f;

    friend class FearWormBattleState;
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void UpdateHeadAimDirection(FearWormBattleState& battle, FearWormEnemy& boss);
    void FireChargedBullet(FearWormBattleState& battle, FearWormEnemy& boss);
    int32_t shotCount_ = 0;
    float elapsedSeconds_ = 0.0f;
    float shotTimerSeconds_ = 0.0f;
    float effectTimerSeconds_ = 0.0f;
    bool isActive_ = false;
    bool isFiring_ = false;
};
