#pragma once

class IceJellyfish;

enum class JellyfishPatternType {
    FrozenSweep, IcicleBloom, TentacleThrust, BlizzardCurrent, CrystalPrison, AbsoluteZero
};

// 攻撃の開始・更新・終了を、現在の状態への基底ポインタから呼び出す。
class JellyfishAttackPattern {
public:
    virtual ~JellyfishAttackPattern() = default;
    virtual JellyfishPatternType GetType() const = 0;
    virtual void Enter(IceJellyfish& boss) const;
    virtual void Update(IceJellyfish& boss) const = 0;
    virtual void Exit(IceJellyfish& boss) const {}
    virtual float GetDuration(int phase) const = 0;
};

// 状態オブジェクトは不変。攻撃中のタイマー・HPなどは各クラゲが保持する。
const JellyfishAttackPattern& GetJellyfishAttackPattern(JellyfishPatternType type);
