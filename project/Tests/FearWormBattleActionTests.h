#pragma once

#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include <array>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <thread>

inline bool RunFearWormBattleActionTests()
{
    using ActionType = FearWormBattleState::ActionType;
    FearWormEnemy boss;
    FearWormEnemy otherBoss;
    Player player;
    boss.player_ = &player;
    boss.segments_.resize(3);
    boss.segments_[0].isHead = true;
    boss.segments_[0].hp = boss.kHeadHp;
    boss.segments_[1].hp = boss.kBodyHp;
    boss.segments_[2].hp = boss.kBodyHp;
    boss.ChangeState(FearWormEnemy::BossState::Battle);
    FearWormBattleState& battle = boss.battleState_;
    if (battle.currentAction_ != &battle.orbitShotState_
        || battle.currentAction_ == otherBoss.battleState_.currentAction_) {
        return false;
    }

    // 行動を切り替えたときだけ初期化し、同じ行動の指定では時刻を戻さない。
    battle.movementPatternTimer_ = 1.0f;
    battle.orbitShotState_.fireTimer_ = 10;
    battle.ChangeAction(ActionType::OrbitShot, boss);
    if (battle.movementPatternTimer_ != 1.0f || battle.orbitShotState_.fireTimer_ != 10) {
        return false;
    }
    const std::array<ActionType, 5> expectedActions = {
        ActionType::WeaveSpread, ActionType::DriftBurst, ActionType::CoilBarrage,
        ActionType::HeadCharge, ActionType::OrbitShot
    };
    for (ActionType expectedAction : expectedActions) {
        battle.FinishAction(boss);
        if (battle.actionType_ != expectedAction || battle.movementPatternTimer_ != 0.0f) {
            return false;
        }
    }
    if (battle.headChargeState_.isActive_ || battle.headChargeState_.isFiring_) {
        return false;
    }

    // 通常攻撃の時間切れで、移動と攻撃をまとめて次の行動へ切り替える。
    battle.movementPatternTimer_ = battle.movementPatternDuration_;
    battle.Update(boss);
    if (battle.actionType_ != ActionType::WeaveSpread
        || battle.movementPattern_ != FearWormBattleState::MovementPattern::Weave) {
        return false;
    }

    // ミサイルの待機時間が満了しても、その場では割り込まない。
    battle.missileAttackTimer_ = battle.kMissileAttackInterval;
    battle.Update(boss);
    if (battle.actionType_ != ActionType::WeaveSpread) {
        return false;
    }
    battle.FinishAction(boss);
    if (battle.actionType_ != ActionType::LineMissile || battle.missileAttackTimer_ != 0.0f) {
        return false;
    }
    battle.lineMissileState_.targetIndices_ = { 0, 1, 2 };
    battle.FinishAction(boss);
    if (battle.actionType_ != ActionType::OrbitShot || !battle.lineMissileState_.targetIndices_.empty()) {
        return false;
    }

    // 別行動へ移った後に、前の連射予約やチャージ表示を残さない。
    battle.ChangeAction(ActionType::DriftBurst, boss);
    battle.driftBurstState_.shotsRemaining_ = 2;
    battle.driftBurstState_.shotTimerSeconds_ = 0.1f;
    battle.ChangeAction(ActionType::CoilBarrage, boss);
    if (battle.driftBurstState_.shotsRemaining_ != 0 || battle.driftBurstState_.shotTimerSeconds_ != 0.0f) {
        return false;
    }
    battle.ChangeAction(ActionType::HeadCharge, boss);
    // 演出をStateへ移しても、統括側の窓口から溜め・発射・終了後を取得できる。
    const Vector4 baseHeadColor = { 0.2f, 0.3f, 0.4f, 1.0f };
    Vector4 headColor = battle.HeadAttackColor(baseHeadColor);
    if (headColor.x != 1.0f || headColor.y != 0.48f || headColor.z != 0.08f
        || headColor.w != 1.0f || std::abs(battle.ApplyHeadChargePulse(1.0f) - 1.12f) > 0.0001f) {
        return false;
    }
    battle.headChargeState_.isFiring_ = true;
    battle.headChargeState_.elapsedSeconds_ = 0.05f;
    headColor = battle.HeadAttackColor(baseHeadColor);
    const float expectedPulse = 1.12f + std::sin(1.2f) * 0.08f;
    if (headColor.x != 1.0f || headColor.y != 0.08f || headColor.z != 0.02f
        || std::abs(battle.ApplyHeadChargePulse(1.0f) - expectedPulse) > 0.0001f) {
        return false;
    }
    battle.ChangeAction(ActionType::OrbitShot, boss);
    headColor = battle.HeadAttackColor(baseHeadColor);
    if (battle.headChargeState_.isActive_ || headColor.x != baseHeadColor.x
        || headColor.y != baseHeadColor.y || headColor.z != baseHeadColor.z
        || headColor.w != baseHeadColor.w || battle.ApplyHeadChargePulse(1.0f) != 1.0f) {
        return false;
    }

    // 激昂は一度だけ実行し、その後は通常行動へ復帰する。
    for (FearWormEnemy::Segment& segment : boss.segments_) {
        segment.hp = 1.0f;
    }
    battle.Update(boss);
    if (battle.actionType_ != ActionType::SpiralBarrage || !battle.hasTriggeredMadMode_) {
        return false;
    }
    // 更新した時間・角度をState自身が渡し、威嚇中と渦移動中の座標を維持する。
    const std::array<float, 3> spiralElapsedSeconds = { 1.0f, 2.0f, 4.5f };
    const std::array<float, 3> spiralAnglesRadians = { 0.4f, 0.7f, 1.2f };
    for (size_t phaseIndex = 0; phaseIndex < spiralElapsedSeconds.size(); phaseIndex++) {
        battle.spiralBarrageState_.elapsedSeconds_ = spiralElapsedSeconds[phaseIndex];
        battle.spiralBarrageState_.spiralAngle_ = spiralAnglesRadians[phaseIndex];
        const Vector3 startPosition = { 11.0f, 14.0f, 20.0f };
        boss.segments_[0].position = startPosition;
        const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
        const float expectedElapsedSeconds = spiralElapsedSeconds[phaseIndex] + deltaTimeSeconds;
        const float expectedAngleRadians = spiralAnglesRadians[phaseIndex]
            + deltaTimeSeconds * 2.5f * boss.MovementSpeedRate();
        Vector3 expectedTarget = player.GetTranslate();
        expectedTarget.y += 7.0f;
        expectedTarget.z += boss.parallelForwardOffset_ + 50.0f;
        if (expectedElapsedSeconds >= 2.0f) {
            const float spiralRadius = (expectedElapsedSeconds - 2.0f) * 5.0f;
            expectedTarget.x += std::cos(expectedAngleRadians) * spiralRadius;
            expectedTarget.y += std::sin(expectedAngleRadians) * spiralRadius;
        }
        const Vector3 expectedPosition = Lerp(startPosition, expectedTarget,
            boss.moveSpeed_ * boss.MovementSpeedRate());
        battle.spiralBarrageState_.Update(battle, boss);
        if (Vector3LengthSquared(boss.segments_[0].position - expectedPosition) > 0.000001f
            || Vector3LengthSquared(boss.transform_.translate - expectedPosition) > 0.000001f) {
            return false;
        }
    }
    // 共通計算はStateの内部値ではなく、呼び出し元から受け取った値を使う。
    const Vector3 explicitTarget = battle.MovementTargetPosition(boss, player.GetTranslate(), 3.0f, 0.0f);
    const Vector3 expectedExplicitTarget = player.GetTranslate()
        + Vector3 { 5.0f, 7.0f, boss.parallelForwardOffset_ + 50.0f };
    if (Vector3LengthSquared(explicitTarget - expectedExplicitTarget) > 0.000001f) {
        return false;
    }
    battle.spiralBarrageState_.elapsedSeconds_ = 7.0f;
    battle.Update(boss);
    if (battle.actionType_ != ActionType::OrbitShot || battle.IsMadModeActive()) {
        return false;
    }
    battle.Update(boss);
    if (battle.actionType_ != ActionType::OrbitShot) {
        return false;
    }

    // 胴体全滅を激昂より優先し、ビーム待機中も通常射撃を混ぜない。
    battle.ChangeAction(ActionType::DriftBurst, boss);
    battle.driftBurstState_.shotsRemaining_ = 2;
    battle.hasTriggeredMadMode_ = false;
    boss.segments_[1].isAlive = false;
    boss.segments_[2].isAlive = false;
    battle.Update(boss);
    if (battle.actionType_ != ActionType::Beam || battle.driftBurstState_.shotsRemaining_ != 0
        || battle.headChargeState_.isActive_ || battle.IsMadModeActive()) {
        return false;
    }
    boss.ChangeState(FearWormEnemy::BossState::Wait);
    if (battle.currentAction_ != nullptr || battle.beamActionState_.beam_.GetWidth() != 0.0f || battle.IsBeamHittingPlayer()) {
        return false;
    }
    boss.SetPosition({ 0.0f, 0.0f, 0.0f });
    if (battle.hasTriggeredMadMode_ || battle.missileAttackTimer_ != 0.0f) {
        return false;
    }
    // 速度倍率が1以外でも、直接更新と統括経由の両方で時間を一度だけ進める。
    FearWormEnemy timingBoss;
    timingBoss.player_ = &player;
    timingBoss.segments_.resize(3);
    timingBoss.segments_[0].isHead = true;
    timingBoss.segments_[0].hp = timingBoss.kHeadHp * 0.5f;
    timingBoss.segments_[1].hp = timingBoss.kBodyHp * 0.5f;
    timingBoss.segments_[2].hp = timingBoss.kBodyHp * 0.5f;
    timingBoss.ChangeState(FearWormEnemy::BossState::Battle);
    const float timingSpeedRate = timingBoss.MovementSpeedRate();
    if (timingSpeedRate <= 1.0f) {
        return false;
    }
    TimeManager* timeManager = TimeManager::GetInstance();
    const float previousMaxDeltaSeconds = timeManager->GetMaxDeltaTime();
    timeManager->SetMaxDeltaTime(0.001f);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    timeManager->Update();
    const float frameDeltaSeconds = timeManager->GetDeltaTime();
    if (frameDeltaSeconds <= 0.0f) {
        return false;
    }
    const std::array<ActionType, 8> timingActions = {
        ActionType::OrbitShot, ActionType::WeaveSpread, ActionType::DriftBurst,
        ActionType::CoilBarrage, ActionType::HeadCharge, ActionType::LineMissile,
        ActionType::SpiralBarrage, ActionType::Beam
    };
    FearWormBattleState& timingBattle = timingBoss.battleState_;
    for (const ActionType actionType : timingActions) {
        timingBattle.ChangeAction(actionType, timingBoss);
        if (actionType == ActionType::Beam) {
            timingBoss.segments_[1].isAlive = false;
            timingBoss.segments_[2].isAlive = false;
        }
        float expectedActionSeconds = frameDeltaSeconds * timingBoss.MovementSpeedRate();
        if (actionType == ActionType::CoilBarrage || actionType == ActionType::SpiralBarrage) {
            expectedActionSeconds = frameDeltaSeconds;
        }
        Vector3 expectedSpiralPosition {};
        if (actionType == ActionType::SpiralBarrage) {
            timingBattle.spiralBarrageState_.elapsedSeconds_ = 3.0f;
            timingBattle.spiralBarrageState_.spiralAngle_ = 0.8f;
            const float nextAngleRadians = 0.8f + frameDeltaSeconds * 2.5f * timingBoss.MovementSpeedRate();
            const Vector3 nextTarget = timingBattle.BattleTarget(timingBoss, player.GetTranslate(),
                3.0f + frameDeltaSeconds, nextAngleRadians);
            expectedSpiralPosition = Lerp(timingBoss.segments_[0].position, nextTarget,
                timingBoss.moveSpeed_ * timingBoss.MovementSpeedRate());
        }
        timingBattle.movementPatternTimer_ = 0.0f;
        timingBattle.currentAction_->Update(timingBattle, timingBoss);
        if (std::abs(timingBattle.movementPatternTimer_ - expectedActionSeconds) > 0.000001f) {
            return false;
        }
        if (actionType == ActionType::SpiralBarrage
            && Vector3LengthSquared(timingBoss.segments_[0].position - expectedSpiralPosition) > 0.000001f) {
            return false;
        }
        timingBattle.movementPatternTimer_ = 0.0f;
        timingBattle.missileAttackTimer_ = 0.0f;
        timingBattle.Update(timingBoss);
        // 弾モデル未初期化のHeadChargeは終了する。終了時の時間リセットも維持する。
        if (actionType == ActionType::HeadCharge) {
            if (timingBattle.actionType_ != ActionType::OrbitShot || timingBattle.movementPatternTimer_ != 0.0f) {
                return false;
            }
        } else if (std::abs(timingBattle.movementPatternTimer_ - expectedActionSeconds) > 0.000001f) {
            return false;
        }
        float expectedMissileSeconds = frameDeltaSeconds;
        if (actionType == ActionType::LineMissile || actionType == ActionType::SpiralBarrage
            || actionType == ActionType::Beam) {
            expectedMissileSeconds = 0.0f;
        }
        if (std::abs(timingBattle.missileAttackTimer_ - expectedMissileSeconds) > 0.000001f) {
            return false;
        }
    }
    timeManager->SetMaxDeltaTime(previousMaxDeltaSeconds);
    timeManager->Initialize();

    std::puts("FearWorm battle actions: cycle, priorities, cancellation, timing and reset passed.");
    return true;
}
