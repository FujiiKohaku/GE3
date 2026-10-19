#pragma once

#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Object3d.h"
#include <cstdio>

inline bool RunFearWormStateTests()
{
    using BossState = FearWormEnemy::BossState;
    FearWormEnemy boss;
    FearWormEnemy otherBoss;
    if (boss.GetBossState() != BossState::Wait
        || boss.currentState_ != &boss.waitState_
        || otherBoss.currentState_ != &otherBoss.waitState_
        || boss.currentState_ == otherBoss.currentState_) {
        return false;
    }

    // プレイヤー不在時は戦闘を進めず、登場状態からは待機へ戻る。
    boss.ChangeState(BossState::Battle);
    boss.UpdateState();
    if (boss.GetBossState() != BossState::Battle
        || boss.currentState_ != &boss.battleState_) { return false; }
    boss.ChangeState(BossState::Entry);
    boss.UpdateState();
    if (boss.GetBossState() != BossState::Wait) { return false; }

    Player player;
    boss.player_ = &player;
    boss.segments_.resize(2);
    boss.segments_[0].object = std::make_unique<Object3d>();
    boss.entryState_.elapsedSeconds_ = 42.0f;

    // 待機の更新が登場へ切り替え、登場の開始処理を一度だけ実行する。
    boss.UpdateState();
    if (boss.GetBossState() != BossState::Entry
        || boss.currentState_ != &boss.entryState_ || boss.entryState_.elapsedSeconds_ != 0.0f) {
        return false;
    }
    boss.entryState_.elapsedSeconds_ = 1.0f;
    boss.ChangeState(BossState::Entry);
    if (boss.entryState_.elapsedSeconds_ != 1.0f) { return false; }

    // 登場終了で戦闘へ進み、別のボスの状態には影響しない。
    boss.entryState_.elapsedSeconds_ = FearWormEntryState::CalculateEntryDurationSeconds(boss.segments_.size());
    boss.entryState_.hasPlayedSplash_ = true;
    boss.UpdateState();
    if (boss.GetBossState() != BossState::Battle
        || otherBoss.GetBossState() != BossState::Wait) {
        return false;
    }

    // 死亡の開始処理が攻撃を終了し、胴体を非表示にする。
    boss.isHeadChargeActive_ = true;
    boss.isHeadChargeFiring_ = true;
    boss.beamState_ = FearWormEnemy::BeamState::Fire;
    boss.beamCurrentLength_ = 100.0f;
    boss.beamCurrentWidth_ = 3.0f;
    boss.OnDeath();
    if (boss.GetBossState() != BossState::Dead
        || boss.currentState_ != &boss.deadState_
        || boss.isHeadChargeActive_ || boss.isHeadChargeFiring_
        || boss.beamState_ != FearWormEnemy::BeamState::Wait
        || boss.beamCurrentLength_ != 0.0f || boss.beamCurrentWidth_ != 0.0f
        || boss.segments_[1].isAlive || boss.isDeathSequenceFinished_) {
        return false;
    }

    // 撃破通知を繰り返しても、死亡演出の経過と完了状態をリセットしない。
    boss.deathSequenceTimer_ = 0.75f;
    boss.isDeathSequenceFinished_ = true;
    boss.OnDeath();
    boss.UpdateState();
    if (boss.deathSequenceTimer_ != 0.75f || !boss.IsDeathSequenceFinished()) {
        return false;
    }

    boss.SetPosition({ 10.0f, 20.0f, 30.0f });
    if (boss.GetBossState() != BossState::Wait
        || boss.currentState_ != &boss.waitState_
        || otherBoss.currentState_ != &otherBoss.waitState_) {
        return false;
    }
    boss.ChangeState(static_cast<BossState>(99));
    if (boss.GetBossState() != BossState::Wait) { return false; }

    std::puts("FearWorm states: dispatch, entry, battle, death and reset passed.");
    return true;
}
