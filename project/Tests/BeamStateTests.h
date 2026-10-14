#pragma once
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include <cmath>

inline bool RunBeamStateTests()
{
    FearWormEnemy boss;
    Player player;
    boss.player_ = &player;
    boss.segments_.resize(1);
    boss.segments_[0].position = { 0.0f, 0.0f, 10.0f };
    boss.ChangeBeamState(FearWormEnemy::BeamState::Wait);
    boss.beamTimer_ = 3.99f;
    boss.UpdateBeamState(0.01f);
    if (boss.beamState_ != FearWormEnemy::BeamState::Wait) return false;
    boss.beamTimer_ = 4.0f;
    boss.UpdateBeamState(0.01f);
    if (boss.beamState_ != FearWormEnemy::BeamState::Charge || boss.beamTimer_ != 0.0f) return false;
    boss.beamTimer_ = 1.49f;
    boss.UpdateBeamState(0.01f);
    if (boss.beamState_ != FearWormEnemy::BeamState::Charge || boss.beamCurrentWidth_ != 0.0f) return false;
    boss.beamTimer_ = 1.5f;
    boss.UpdateBeamState(0.01f);
    if (boss.beamState_ != FearWormEnemy::BeamState::Fire || boss.beamTimer_ != 0.0f) return false;
    boss.beamTimer_ = 2.99f;
    boss.UpdateBeamState(0.1f);
    if (boss.beamState_ != FearWormEnemy::BeamState::Fire || boss.beamCurrentLength_ != 100.0f ||
        boss.beamCurrentWidth_ != 3.5f) return false;
    boss.beamTimer_ = 3.0f;
    boss.UpdateBeamState(0.5f);
    if (boss.beamState_ != FearWormEnemy::BeamState::FadeOut || boss.beamTimer_ != 0.0f ||
        boss.beamCurrentLength_ != 400.0f) return false;
    const int hp = player.GetCurrentHp();
    boss.beamTimer_ = 0.25f;
    boss.UpdateBeamState(0.25f);
    if (std::abs(boss.beamCurrentWidth_ - 1.5f) > 0.0001f || player.GetCurrentHp() != hp) return false;
    boss.beamTimer_ = 0.5f;
    boss.UpdateBeamState(0.25f);
    if (boss.beamState_ != FearWormEnemy::BeamState::Wait || boss.beamTimer_ != 0.0f) return false;
    boss.UpdateBeamState(0.01f);
    if (boss.beamCurrentWidth_ != 0.0f || boss.beamCurrentLength_ != 0.0f) return false;
    boss.beamTimer_ = 2.0f;
    boss.ChangeBeamState(FearWormEnemy::BeamState::Wait);
    return boss.beamTimer_ == 2.0f; // 同じ状態へ指定しても再初期化しない。
}
