#pragma once
#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include <cmath>

inline bool RunJellyfishAttackPatternTests()
{
    constexpr float durations[] = { 6.0f, 6.2f, 6.5f, 6.5f, 7.5f, 8.0f };
    for (int i = 0; i < 6; ++i) {
        const auto type = static_cast<JellyfishPatternType>(i);
        const JellyfishAttackPattern* pattern = &GetJellyfishAttackPattern(type);
        if (pattern->GetType() != type || std::abs(pattern->GetDuration(1) - durations[i]) > 0.0001f) {
            return false;
        }
    }
    if (GetJellyfishAttackPattern(JellyfishPatternType::FrozenSweep).GetDuration(3) != 5.4f) return false;

    IceJellyfish boss;
    boss.BeginPattern(JellyfishPatternType::CrystalPrison);
    for (float hp : boss.crystalHp_) if (hp != 6.0f) return false;
    boss.BeginPattern(JellyfishPatternType::IcicleBloom);
    for (float hp : boss.crystalHp_) if (hp != 0.0f) return false;
    boss.UpdatePattern(1.4f);
    if (boss.firedWave_ != 1) return false;
    boss.UpdatePattern(0.9f);
    if (boss.firedWave_ != 2) return false;
    // 初期フェーズの攻撃順と、時間境界での遷移・タイマー初期化。
    boss.patternIndex_ = 0;
    boss.BeginPattern(JellyfishPatternType::FrozenSweep);
    boss.UpdatePattern(6.0f);
    return boss.GetAttackPattern() == JellyfishPatternType::IcicleBloom &&
        boss.attackTimer_ == 0.0f && boss.previousAttackTimer_ == 0.0f && boss.firedWave_ == 0;
}
