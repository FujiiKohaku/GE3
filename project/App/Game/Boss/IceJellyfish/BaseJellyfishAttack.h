#pragma once
#include <array>
class IceJellyfish;
inline constexpr float kIcePillarRevealSeconds = 2.00f;

// 攻撃は互いに独立して進行する。実行状態はクラゲごとに保持する。
class BaseJellyfishAttack {
public:
    virtual ~BaseJellyfishAttack() = default;
    virtual void Start(IceJellyfish& boss) const = 0;
    virtual void Update(IceJellyfish& boss, float dt) const = 0;
};

const std::array<const BaseJellyfishAttack*, 3>& GetJellyfishAttacks();
