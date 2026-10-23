#pragma once

#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/3D/Object3d.h"
#include "App/Game/Boss/Encounter/BossEncounterController.h"
#include "FearWormTimingTests.h"
#include <cstdio>
#include <utility>

// 描画資源を使わず、統括からの更新回数と死亡中の時間経過を観測する。
class BossUpdateProbe final : public BaseBoss {
public:
    void Update() override
    {
        updateCount_++;
        if (IsDead() && !IsDeathSequenceFinished()) {
            deathElapsedSeconds_ += TimeManager::GetInstance()->GetDeltaTime();
        }
    }
    bool IsDeathSequenceFinished() const override { return deathElapsedSeconds_ >= 1.6f; }
    bool IsMadModeActive() const override { return false; }
    bool IsBeamHittingPlayer() const override { return false; }
    float GetHeadHpFraction() const override { return 1.0f; }
    float GetBodyHpFraction() const override { return 1.0f; }
    int32_t GetUpdateCount() const { return updateCount_; }
    float GetDeathElapsedSeconds() const { return deathElapsedSeconds_; }

private:
    int32_t updateCount_ = 0;
    float deathElapsedSeconds_ = 0.0f;
};

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
    boss.battleState_.ChangeAction(FearWormBattleState::ActionType::DriftBurst, boss);
    boss.battleState_.driftBurstState_.shotsRemaining_ = 2;
    boss.battleState_.beamActionState_.phase_ = FearWormBeamState::Phase::Fire;
    boss.battleState_.beamActionState_.beam_.SetDimensions(100.0f, 3.0f);
    boss.OnDeath();
    if (boss.GetBossState() != BossState::Dead
        || boss.currentState_ != &boss.deadState_
        || boss.battleState_.headChargeState_.isActive_ || boss.battleState_.headChargeState_.isFiring_
        || boss.battleState_.driftBurstState_.shotsRemaining_ != 0
        || boss.battleState_.beamActionState_.phase_ != FearWormBeamState::Phase::Wait
        || boss.battleState_.beamActionState_.beam_.GetLength() != 0.0f || boss.battleState_.beamActionState_.beam_.GetWidth() != 0.0f
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
    FearWormBattleState& battle = boss.battleState_;
    battle.orbitShotState_.elapsedSeconds_ = 1.0f;
    battle.weaveSpreadState_.elapsedSeconds_ = 2.0f;
    battle.driftBurstState_.elapsedSeconds_ = 3.0f;
    battle.coilBarrageState_.elapsedSeconds_ = 4.0f;
    battle.lineMissileState_.formationElapsedSeconds_ = 0.5f;
    otherBoss.battleState_.orbitShotState_.elapsedSeconds_ = 7.0f;
    battle.orbitShotState_.fireElapsedSeconds_ = 9;
    battle.orbitShotState_.isFinished_ = true;
    battle.weaveSpreadState_.fireElapsedSeconds_ = 8;
    battle.weaveSpreadState_.isFinished_ = true;
    battle.coilBarrageState_.nextFireSeconds_ = 3.0f;
    battle.coilBarrageState_.isFinished_ = true;
    battle.driftBurstState_.fireElapsedSeconds_ = 7;
    battle.driftBurstState_.segmentIndex_ = 1;
    battle.driftBurstState_.direction_ = { 1.0f, 0.0f, 0.0f };
    battle.driftBurstState_.isFinished_ = true;
    boss.battleState_.driftBurstState_.shotsRemaining_ = 2;
    boss.battleState_.driftBurstState_.shotTimerSeconds_ = 0.1f;
    battle.headChargeState_.isActive_ = true;
    battle.headChargeState_.isFiring_ = true;
    battle.headChargeState_.elapsedSeconds_ = 2.0f;
    battle.headChargeState_.shotTimerSeconds_ = 0.4f;
    battle.headChargeState_.effectTimerSeconds_ = 0.2f;
    battle.headChargeState_.shotCount_ = 2;
    battle.headChargeState_.isFinished_ = true;
    battle.lineMissileState_.phase_ = FearWormLineMissileState::Phase::Recovery;
    battle.lineMissileState_.phaseTimerSeconds_ = 1.0f;
    battle.lineMissileState_.shotCount_ = 1;
    battle.lineMissileState_.isFinished_ = true;
    boss.battleState_.lineMissileState_.targetIndices_ = { 1 };
    battle.lineMissileState_.targetIndices_.reserve(8);
    const size_t targetCapacity = battle.lineMissileState_.targetIndices_.capacity();
    const size_t* targetStorage = battle.lineMissileState_.targetIndices_.data();
    battle.spiralBarrageState_.elapsedSeconds_ = 5.0f;
    battle.spiralBarrageState_.spiralAngle_ = 1.0f;
    battle.spiralBarrageState_.barrageAngle_ = 2.0f;
    battle.spiralBarrageState_.fireElapsedSeconds_ = 2;
    battle.spiralBarrageState_.isFinished_ = true;
    battle.beamActionState_.phase_ = FearWormBeamState::Phase::Fire;
    battle.beamActionState_.elapsedSeconds_ = 2.0f;
    battle.beamActionState_.effectTimerSeconds_ = 0.2f;
    battle.beamActionState_.pendingShotEffectCount_ = 1;
    battle.beamActionState_.beam_.SetDimensions(100.0f, 3.5f);
    otherBoss.battleState_.headChargeState_.elapsedSeconds_ = 2.0f;
    otherBoss.battleState_.driftBurstState_.shotsRemaining_ = 1;
    boss.SetPosition({ 10.0f, 20.0f, 30.0f });
    if (boss.GetBossState() != BossState::Wait
        || boss.currentState_ != &boss.waitState_
        || otherBoss.currentState_ != &otherBoss.waitState_
        || boss.battleState_.headChargeState_.isActive_
        || boss.battleState_.currentAction_ != nullptr
        || boss.battleState_.driftBurstState_.shotsRemaining_ != 0
        || boss.battleState_.driftBurstState_.shotTimerSeconds_ != 0.0f
        || !boss.battleState_.lineMissileState_.targetIndices_.empty()
        || otherBoss.battleState_.driftBurstState_.shotsRemaining_ != 1
        || otherBoss.battleState_.headChargeState_.elapsedSeconds_ != 2.0f) {
        return false;
    }
    if (battle.orbitShotState_.fireElapsedSeconds_ != 0 || battle.orbitShotState_.IsFinished()
        || battle.weaveSpreadState_.fireElapsedSeconds_ != 0 || battle.weaveSpreadState_.IsFinished()
        || battle.coilBarrageState_.nextFireSeconds_ != 0.0f || battle.coilBarrageState_.IsFinished()
        || battle.driftBurstState_.fireElapsedSeconds_ != 0 || battle.driftBurstState_.segmentIndex_ != 0
        || battle.driftBurstState_.direction_.x != 0.0f || battle.driftBurstState_.direction_.y != 0.0f
        || battle.driftBurstState_.direction_.z != -1.0f || battle.driftBurstState_.IsFinished()
        || battle.headChargeState_.isFiring_ || battle.headChargeState_.elapsedSeconds_ != 0.0f
        || battle.headChargeState_.shotTimerSeconds_ != 0.0f || battle.headChargeState_.effectTimerSeconds_ != 0.0f
        || battle.headChargeState_.shotCount_ != 0 || battle.headChargeState_.IsFinished()
        || battle.lineMissileState_.phase_ != FearWormLineMissileState::Phase::Formation
        || battle.lineMissileState_.phaseTimerSeconds_ != 0.0f || battle.lineMissileState_.shotCount_ != 0
        || battle.lineMissileState_.IsFinished()
        || battle.lineMissileState_.targetIndices_.capacity() != targetCapacity
        || battle.lineMissileState_.targetIndices_.data() != targetStorage
        || battle.spiralBarrageState_.elapsedSeconds_ != 0.0f || battle.spiralBarrageState_.spiralAngle_ != 0.0f
        || battle.spiralBarrageState_.barrageAngle_ != 0.0f || battle.spiralBarrageState_.fireElapsedSeconds_ != 0
        || battle.spiralBarrageState_.IsFinished()
        || battle.beamActionState_.IsAiming() || battle.beamActionState_.elapsedSeconds_ != 0.0f
        || battle.beamActionState_.effectTimerSeconds_ != 0.0f || battle.beamActionState_.pendingShotEffectCount_ != 0
        || battle.beamActionState_.beam_.GetLength() != 0.0f || battle.beamActionState_.beam_.GetWidth() != 0.0f) {
        return false;
    }
    if (battle.orbitShotState_.elapsedSeconds_ != 0.0f || battle.weaveSpreadState_.elapsedSeconds_ != 0.0f
        || battle.driftBurstState_.elapsedSeconds_ != 0.0f || battle.coilBarrageState_.elapsedSeconds_ != 0.0f
        || battle.lineMissileState_.formationElapsedSeconds_ != 0.0f
        || otherBoss.battleState_.orbitShotState_.elapsedSeconds_ != 7.0f) {
        return false;
    }
    // 繰り返しても予約領域を維持し、部位数が増えたときだけ必要容量を準備する。
    boss.SetPosition({ 10.0f, 20.0f, 30.0f });
    if (battle.lineMissileState_.targetIndices_.data() != targetStorage) {
        return false;
    }
    boss.segments_.resize(targetCapacity + 1);
    boss.SetPosition({ 10.0f, 20.0f, 30.0f });
    if (battle.lineMissileState_.targetIndices_.capacity() < boss.segments_.size()
        || !battle.lineMissileState_.targetIndices_.empty()) {
        return false;
    }
    boss.ChangeState(static_cast<BossState>(99));
    if (boss.GetBossState() != BossState::Wait) { return false; }

    // 描画リソースを必要としない、Initialize共通の状態リセットを検証する。
    // 死亡演出の途中と完了後のどちらからも同じ個体を再利用できること。
    for (const bool deathFinished : { false, true }) {
        boss.SetDead(true);
        boss.deadState_.isDeathSequenceFinished_ = deathFinished;
        boss.deadState_.elapsedSeconds_ = 1.0f;
        boss.moveTime_ = 10.0f;
        boss.vulnerableEffectPlayed_ = true;
        boss.ResetForInitialize();
        boss.SetPosition({ 10.0f, 20.0f, 30.0f });
        if (boss.IsDead() || boss.GetHealth() != FearWormEnemy::kHeadHp
            || boss.GetBossState() != BossState::Wait || boss.currentState_ != &boss.waitState_
            || boss.IsDeathSequenceFinished() || boss.deadState_.elapsedSeconds_ != 0.0f
            || boss.moveTime_ != 0.0f || boss.vulnerableEffectPlayed_
            || boss.battleState_.currentAction_ != nullptr) {
            return false;
        }
    }

    // 死亡後も通常の更新窓口だけで進むことを、統括を通して確認する。
    FearWormTestClock deathClock;
    deathClock.SetDelta(1.0f / 60.0f);
    BossEncounterController encounter;
    encounter.bossType_ = "FearWorm";
    encounter.isSpawned_ = true;
    auto ownedProbe = std::make_unique<BossUpdateProbe>();
    BossUpdateProbe& updateProbe = *ownedProbe;
    encounter.activeBoss_ = std::move(ownedProbe);
    encounter.Update(0.0f);
    if (updateProbe.GetUpdateCount() != 1 || updateProbe.GetDeathElapsedSeconds() != 0.0f) {
        return false;
    }
    updateProbe.SetDead(true);
    const float frameDeltaSeconds = TimeManager::GetInstance()->GetDeltaTime();
    for (int32_t stepIndex = 0; stepIndex < 48; stepIndex++) {
        encounter.Update(0.0f);
    }
    if (updateProbe.GetUpdateCount() != 49 || updateProbe.IsDeathSequenceFinished()
        || std::abs(updateProbe.GetDeathElapsedSeconds() - frameDeltaSeconds * 48.0f) > 0.00001f) {
        return false;
    }
    for (int32_t stepIndex = 0; stepIndex < 50; stepIndex++) {
        encounter.Update(0.0f);
    }
    if (updateProbe.GetUpdateCount() != 99 || !updateProbe.IsDeathSequenceFinished()) {
        return false;
    }

    // 次のUpdateを待たず、再初期化直後から前回の通知を返さない。
    encounter.didSpawnThisFrame_ = true;
    encounter.didEnterMadModeThisFrame_ = true;
    encounter.isBeamHittingPlayer_ = true;
    encounter.didExtendRailThisFrame_ = true;
    encounter.Initialize("None", 0.0f, {}, nullptr, nullptr, nullptr, nullptr, nullptr, false, 0.0f);
    if (encounter.GetActiveBoss() != nullptr || encounter.IsSpawned()
        || encounter.DidSpawnThisFrame() || encounter.DidEnterMadModeThisFrame()
        || encounter.IsBeamHittingPlayer() || encounter.DidExtendRailThisFrame()) {
        return false;
    }

    std::puts("FearWorm states: dispatch, entry, battle, single death update and reset passed.");
    return true;
}
