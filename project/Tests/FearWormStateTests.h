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
    boss.battleState_.isHeadChargeActive_ = true;
    boss.battleState_.isHeadChargeFiring_ = true;
    boss.battleState_.beamState_ = FearWormEnemy::BeamState::Fire;
    boss.battleState_.beamCurrentLength_ = 100.0f;
    boss.battleState_.beamCurrentWidth_ = 3.0f;
    boss.OnDeath();
    if (boss.GetBossState() != BossState::Dead
        || boss.currentState_ != &boss.deadState_
        || boss.battleState_.isHeadChargeActive_ || boss.battleState_.isHeadChargeFiring_
        || boss.battleState_.beamState_ != FearWormEnemy::BeamState::Wait
        || boss.battleState_.beamCurrentLength_ != 0.0f || boss.battleState_.beamCurrentWidth_ != 0.0f
        || boss.segments_[1].isAlive || boss.deadState_.isDeathSequenceFinished_) {
        return false;
    }

    // 撃破通知を繰り返しても、死亡演出の経過と完了状態をリセットしない。
    boss.deadState_.elapsedSeconds_ = 0.75f;
    boss.deadState_.isDeathSequenceFinished_ = true;
    boss.OnDeath();
    boss.UpdateState();
    if (boss.deadState_.elapsedSeconds_ != 0.75f || !boss.IsDeathSequenceFinished()) {
        return false;
    }

    // 配置し直したボスの攻撃状態だけを初期化し、別個体には影響させない。
    boss.battleState_.isHeadChargeActive_ = true;
    boss.battleState_.isFiringMissilesSequence_ = true;
    boss.battleState_.missileTargetIndices_ = { 1 };
    otherBoss.battleState_.headChargeTimer_ = 2.0f;
    boss.SetPosition({ 10.0f, 20.0f, 30.0f });
    if (boss.GetBossState() != BossState::Wait
        || boss.currentState_ != &boss.waitState_
        || otherBoss.currentState_ != &otherBoss.waitState_
        || boss.battleState_.isHeadChargeActive_
        || boss.battleState_.isFiringMissilesSequence_
        || !boss.battleState_.missileTargetIndices_.empty()
        || otherBoss.battleState_.headChargeTimer_ != 2.0f) {
        return false;
    }
    boss.ChangeState(static_cast<BossState>(99));
    if (boss.GetBossState() != BossState::Wait) { return false; }

    std::puts("FearWorm states: dispatch, entry, battle, death and reset passed.");
    return true;
}
