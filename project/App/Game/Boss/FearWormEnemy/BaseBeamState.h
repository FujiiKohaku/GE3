#pragma once
class FearWormEnemy;
enum class FearWormBeamState { Wait, Charge, Fire, FadeOut };

class BaseBeamState {
public:
    virtual ~BaseBeamState() = default;
    virtual FearWormBeamState GetType() const = 0;
    virtual void Enter(FearWormEnemy& boss) const;
    virtual void Update(FearWormEnemy& boss, float dt) const = 0;
    virtual void Exit(FearWormEnemy&) const {}
};

const BaseBeamState& GetBeamState(FearWormBeamState type);
