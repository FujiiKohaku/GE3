#pragma once

class IceJellyfish;

// 各個体が所有する、大きな状態の開始・更新・終了処理。
class IceJellyfishState {
public:
    virtual ~IceJellyfishState() = default;
    virtual void Enter(IceJellyfish&) {}
    virtual void Update(IceJellyfish& boss) = 0;
    virtual void Exit(IceJellyfish&) {}
};
