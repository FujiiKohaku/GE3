#pragma once
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include <cmath>
#include <numbers>

inline bool RunJellyfishAttackTests()
{
    IceJellyfish boss;
    boss.tentacleHp_.fill(6.0f);
    // 浮遊のタイミングで一度だけ開始し、波状弾→氷槍→氷柱の順を保つ。
    boss.battleState_.UpdateAttackSequence(boss, -std::numbers::pi_v<float> / 2.0f, 0.0f);
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.0f);
    if (boss.battleState_.nextAttackIndex_ != 1 || boss.pulseBarrageWave_ != 0) return false;
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.0f);
    if (boss.battleState_.nextAttackIndex_ != 1 || boss.pulseBarrageWave_ != 1) return false;
    boss.battleState_.UpdateAttackSequence(boss, -std::numbers::pi_v<float> / 2.0f, 0.18f);
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.0f);
    if (boss.battleState_.nextAttackIndex_ != 2 || boss.iceSpearVolley_ != 0 || boss.pulseBarrageWave_ != 2) return false;

    Player player;
    boss.player_ = &player;
    boss.battleState_.UpdateAttackSequence(boss, -std::numbers::pi_v<float> / 2.0f, 0.0f);
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.0f);
    if (boss.battleState_.nextAttackIndex_ != 0 || boss.icePillarWave_ != 0) return false;
    boss.player_ = nullptr; // 描画や弾を生成せず、並行動作と時間を検証する。
    boss.battleState_.UpdateAttackSequence(boss, 0.0f, 0.18f);
    if (boss.pulseBarrageWave_ != -1 || std::abs(boss.iceSpearTimer_ - 0.18f) > 0.0001f ||
        std::abs(boss.icePillarTimer_ - 0.18f) > 0.0001f) return false;
    boss.UpdateIceSpearAttack(0.82f);
    boss.UpdateIceSpearAttack(0.20f);
    boss.UpdateIceSpearAttack(0.21f);
    if (boss.iceSpearVolley_ != 3) return false;
    boss.UpdateIceSpearAttack(1.0f);
    if (boss.iceSpearVolley_ != -1 || boss.iceSpearPoseWeight_ != 0.0f) return false;
    boss.UpdateIcePillarAttack(1.82f);
    if (!boss.icePillarModelSpawnedForWave_) return false;
    boss.player_ = &player;
    const float pillarTime = boss.icePillarTimer_;
    boss.StartIcePillarAttack();
    if (boss.icePillarTimer_ != pillarTime) return false; // 氷柱の実行中は再開始しない。
    boss.player_ = nullptr;
    boss.UpdateIcePillarAttack(7.0f);
    if (boss.icePillarWave_ != 1 || boss.icePillarTimer_ != 0.0f) return false;
    boss.UpdateIcePillarAttack(9.0f);
    boss.UpdateIcePillarAttack(9.0f);
    if (boss.icePillarWave_ != -1) return false;
    boss.tentacleHp_.fill(0.0f);
    boss.StartIceSpearAttack();
    return boss.iceSpearVolley_ == -1;
}
