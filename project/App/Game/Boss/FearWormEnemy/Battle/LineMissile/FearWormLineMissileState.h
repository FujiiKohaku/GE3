#pragma once

#include "../State/FearWormBattleAction.h"
#include "Engine/Math/MathStruct.h"
#include <cstdint>
#include <cstddef>
#include <vector>

class FearWormLineMissileState final : public FearWormBattleAction {
public:
    void Enter(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Update(FearWormBattleState& battle, FearWormEnemy& boss) override;
    void Exit(FearWormBattleState& battle, FearWormEnemy& boss) override;

private:
    static constexpr float kMissileAimWaitDurationSeconds = 2.0f;   // 整列完了（前準備）を待つ時間（秒）
    static constexpr float kMissileFireIntervalSeconds = 0.45f;    // ミサイルの発射間隔（秒）
    static constexpr float kMissilePostFireCooldownSeconds = 1.8f; // 撃ち終わった後の隙（秒）

    friend class FearWormBattleState;
    friend bool RunFearWormStateTests();
    friend bool RunFearWormBattleActionTests();
    void FireSingleMissile(FearWormBattleState& battle, FearWormEnemy& boss, size_t segmentIndex);
    enum class Phase { Formation, Fire, Recovery };
    Phase phase_ = Phase::Formation;
    float phaseTimerSeconds_ = 0.0f;
    int32_t shotCount_ = 0;
    std::vector<size_t> targetIndices_;
};
