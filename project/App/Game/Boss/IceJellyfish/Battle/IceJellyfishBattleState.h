#pragma once

#include "../State/IceJellyfishState.h"
#include "PulseBarrage/IceJellyfishPulseBarrageAttack.h"
#include "IceSpear/IceJellyfishIceSpearAttack.h"
#include "IcePillar/IceJellyfishIcePillarAttack.h"
#include "Engine/Math/MathStruct.h"
#include <array>
#include <cstddef>
#include <cstdint>

// 浮遊移動、並行する3攻撃と、その進行状態を所有する。
class IceJellyfishBattleState final : public IceJellyfishState {
public:
    static constexpr size_t kTentacleCount = 6;
    static constexpr size_t kIcePillarModelCount = IceJellyfishIcePillarAttack::kModelCount;
    void Reset();
    void Update(IceJellyfish& boss) override;
    void Exit(IceJellyfish& boss) override;
    float GetIceSpearPoseWeight() const { return iceSpearAttack_.GetPoseWeight(); }
    bool HasIcePillarModels() const { return icePillarAttack_.HasModels(); }
    const std::array<Vector3, 2>& GetIcePillarPositions() const { return icePillarAttack_.GetPositions(); }
    const std::array<Vector3, kIcePillarModelCount>& GetIcePillarModelPositions() const { return icePillarAttack_.GetModelPositions(); }
    bool IsIcePillarTentacle(size_t tentacle) const;
    float GetIcePillarCharge() const;
    float GetIcePillarModelScaleWeight() const;
    bool IsIceSpearEmitter(const IceJellyfish& boss, size_t tentacle, size_t segment) const;
    bool IsIceSpearTentacleSelected(size_t tentacle) const;
    Vector3 GetIceSpearAimTarget(size_t tentacle) const;

    Vector3 ApplyAttackTentacleDirection(const IceJellyfish& boss,
        const IceJellyfishAttackMotionContext& context, const Vector3& direction) const;
    Vector4 ApplyAttackTentacleColor(const IceJellyfish& boss,
        const IceJellyfishAttackMotionContext& context, const Vector4& baseColor) const;

private:
    friend bool RunJellyfishAttackTests();
    friend bool RunIceJellyfishStateTests();
    void UpdateAttackSequence(IceJellyfish& boss, float floatPhase, float deltaTimeSeconds);
    IceJellyfishPulseBarrageAttack pulseBarrageAttack_;
    IceJellyfishIceSpearAttack iceSpearAttack_;
    IceJellyfishIcePillarAttack icePillarAttack_;
    int32_t nextAttackIndex_ = 0;
    bool isPulseAttackArmed_ = false;
};
