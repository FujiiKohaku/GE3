#pragma once

#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include <array>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <thread>
#include "App/Game/Boss/FearWormEnemy/Battle/State/FearWormTiming.h"
#include "FearWormTimingTests.h"

// 親が特定の攻撃ではなく、現在の行動の問い合わせを使うことを確認する。
class FearWormPresentationProbeAction final : public FearWormBattleAction {
public:
    void Enter(FearWormBattleState&, FearWormEnemy&) override {}
    void Update(FearWormBattleState&, FearWormEnemy&) override {}
    void Exit(FearWormBattleState&, FearWormEnemy&) override {}
    bool ShouldAimHead(const FearWormEnemy&) const override { return true; }
    Vector4 HeadAttackColor(const Vector4&) const override { return {0.1f, 0.7f, 0.9f, 1.0f}; }
    float ApplyHeadChargePulse(float basePulse) const override { return basePulse + 0.35f; }
    float GetFormationBlend() const override { return 0.25f; }
    Vector3 CalculateFormationPosition(const FearWormFormationContext& context,
        const Vector3& followPosition, float formationBlend) const override
    {
        return followPosition + Vector3 { static_cast<float>(context.segmentOrder), formationBlend, 2.0f };
    }
};

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

    {
        const Vector4 kBaseColor{0.2f, 0.3f, 0.4f, 1.0f};
        FearWormBattleState& inactiveBattle = otherBoss.battleState_;
        const Vector4 inactiveColor = inactiveBattle.HeadAttackColor(kBaseColor);
        const Vector4 normalColor = battle.HeadAttackColor(kBaseColor);
        if (inactiveBattle.ShouldAimHead(otherBoss) || battle.ShouldAimHead(boss)
            || inactiveColor.x != kBaseColor.x || inactiveColor.y != kBaseColor.y || inactiveColor.z != kBaseColor.z
            || inactiveColor.w != kBaseColor.w || inactiveBattle.ApplyHeadChargePulse(1.0f) != 1.0f
            || normalColor.x != kBaseColor.x || normalColor.y != kBaseColor.y || normalColor.z != kBaseColor.z
            || normalColor.w != kBaseColor.w || battle.ApplyHeadChargePulse(1.0f) != 1.0f) {
            return false;
        }
        FearWormPresentationProbeAction probeAction;
        FearWormBattleAction* previousAction = battle.currentAction_;
        battle.currentAction_ = &probeAction;
        const Vector4 probeColor = battle.HeadAttackColor(kBaseColor);
        const bool hasDelegatedPresentation = battle.ShouldAimHead(boss)
            && probeColor.x == 0.1f && probeColor.y == 0.7f && probeColor.z == 0.9f && probeColor.w == 1.0f
            && std::abs(battle.ApplyHeadChargePulse(1.0f) - 1.35f) < 0.0001f;
        const Vector3 probeFormation = battle.CalculateFormationPosition(boss, 2,
            { 10.0f, 20.0f, 30.0f }, battle.GetFormationBlend());
        battle.currentAction_ = previousAction;
        if (!hasDelegatedPresentation
            || Vector3LengthSquared(probeFormation - Vector3 { 12.0f, 20.25f, 32.0f }) > 0.000001f) {
            return false;
        }
        battle.ChangeAction(ActionType::Beam, boss);
        battle.beamActionState_.phase_ = FearWormBeamState::Phase::Charge;
        if (battle.ShouldAimHead(boss)) {
            return false;
        }
        boss.segments_[1].isAlive = false;
        boss.segments_[2].isAlive = false;
        if (!battle.ShouldAimHead(boss)) {
            return false;
        }
        battle.beamActionState_.phase_ = FearWormBeamState::Phase::Wait;
        if (battle.ShouldAimHead(boss)) {
            return false;
        }
        boss.segments_[1].isAlive = true;
        boss.segments_[2].isAlive = true;
        battle.ChangeAction(ActionType::OrbitShot, boss);
    }

    // 行動を切り替えたときだけ初期化し、同じ行動の指定では時刻を戻さない。
    battle.orbitShotState_.elapsedSeconds_ = 1.0f;
    battle.orbitShotState_.fireElapsedSeconds_ = 10;
    battle.ChangeAction(ActionType::OrbitShot, boss);
    if (battle.orbitShotState_.elapsedSeconds_ != 1.0f || battle.orbitShotState_.fireElapsedSeconds_ != 10) {
        return false;
    }
    const std::array<ActionType, 5> expectedActions = {
        ActionType::WeaveSpread, ActionType::DriftBurst, ActionType::CoilBarrage,
        ActionType::HeadCharge, ActionType::OrbitShot
    };
    for (ActionType expectedAction : expectedActions) {
        battle.FinishAction(boss);
        if (battle.actionType_ != expectedAction || battle.orbitShotState_.elapsedSeconds_ != 0.0f) {
            return false;
        }
    }
    if (battle.headChargeState_.isActive_ || battle.headChargeState_.isFiring_) {
        return false;
    }

    // 通常攻撃の時間切れで、移動と攻撃をまとめて次の行動へ切り替える。
    battle.orbitShotState_.elapsedSeconds_ = battle.movementPatternDuration_;
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
        || headColor.w != 1.0f || !battle.ShouldAimHead(boss)
        || std::abs(battle.ApplyHeadChargePulse(1.0f) - 1.12f) > 0.0001f) {
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
    if (battle.headChargeState_.isActive_ || battle.ShouldAimHead(boss) || headColor.x != baseHeadColor.x
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
            FearWormTiming::CalculateFollowBlend(boss.moveSpeed_ * boss.MovementSpeedRate(), deltaTimeSeconds));
        battle.spiralBarrageState_.Update(battle, boss);
        if (Vector3LengthSquared(boss.segments_[0].position - expectedPosition) > 0.000001f
            || Vector3LengthSquared(boss.transform_.translate - expectedPosition) > 0.000001f) {
            return false;
        }
    }
    // 共通計算はStateの内部値ではなく、呼び出し元から受け取った値を使う。
    const Vector3 explicitTarget = FearWormSpiralBarrageState::CalculateTarget(battle.CreateMovementContext(boss, player.GetTranslate()), 3.0f, 0.0f);
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
    // 激昂は正常終了だけでなく、中断しても親が待機時間を初期化する。
    battle.ChangeAction(ActionType::SpiralBarrage, boss);
    battle.missileAttackTimer_ = 5.0f;
    battle.ChangeAction(ActionType::OrbitShot, boss);
    if (battle.missileAttackTimer_ != 0.0f || !battle.hasTriggeredMadMode_) {
        return false;
    }
    battle.missileAttackTimer_ = 5.0f;
    battle.ChangeAction(ActionType::LineMissile, boss);
    if (battle.missileAttackTimer_ != 0.0f) {
        return false;
    }
    battle.missileAttackTimer_ = 2.0f;
    battle.ChangeAction(ActionType::LineMissile, boss);
    if (battle.missileAttackTimer_ != 2.0f) {
        return false;
    }
    battle.ChangeAction(ActionType::OrbitShot, boss);
    battle.Reset(boss);
    battle.Enter(boss);
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
    // 各Stateの実際のタイマーを観測する。HeadCharge/Line/Beamは既存の専用時計を再利用する。
    const std::array<float*, 8> actionTimers = {
        &timingBattle.orbitShotState_.elapsedSeconds_, &timingBattle.weaveSpreadState_.elapsedSeconds_,
        &timingBattle.driftBurstState_.elapsedSeconds_, &timingBattle.coilBarrageState_.elapsedSeconds_,
        &timingBattle.headChargeState_.elapsedSeconds_, &timingBattle.lineMissileState_.phaseTimerSeconds_,
        &timingBattle.spiralBarrageState_.elapsedSeconds_, &timingBattle.beamActionState_.elapsedSeconds_
    };
    for (size_t actionIndex = 0; actionIndex < timingActions.size(); actionIndex++) {
        const ActionType actionType = timingActions[actionIndex];
        timingBattle.ChangeAction(actionType, timingBoss);
        if (actionType == ActionType::Beam) {
            timingBoss.segments_[1].isAlive = false;
            timingBoss.segments_[2].isAlive = false;
        }
        float expectedActionSeconds = frameDeltaSeconds;
        if (actionType == ActionType::OrbitShot || actionType == ActionType::WeaveSpread
            || actionType == ActionType::DriftBurst) {
            expectedActionSeconds *= timingBoss.MovementSpeedRate();
        }
        // 弾モデルがないHeadChargeは時計を進める前に終了する。
        if (actionType == ActionType::HeadCharge) {
            expectedActionSeconds = 0.0f;
        }
        Vector3 expectedSpiralPosition {};
        if (actionType == ActionType::SpiralBarrage) {
            timingBattle.spiralBarrageState_.elapsedSeconds_ = 3.0f;
            timingBattle.spiralBarrageState_.spiralAngle_ = 0.8f;
            const float nextAngleRadians = 0.8f + frameDeltaSeconds * 2.5f * timingBoss.MovementSpeedRate();
            const Vector3 nextTarget = FearWormSpiralBarrageState::CalculateTarget(
                timingBattle.CreateMovementContext(timingBoss, player.GetTranslate()), 3.0f + frameDeltaSeconds, nextAngleRadians);
            expectedSpiralPosition = Lerp(timingBoss.segments_[0].position, nextTarget,
                FearWormTiming::CalculateFollowBlend(timingBoss.moveSpeed_ * timingBoss.MovementSpeedRate(), frameDeltaSeconds));
        }
        std::array<float, 8> previousActionSeconds {};
        for (size_t timerIndex = 0; timerIndex < actionTimers.size(); timerIndex++) {
            previousActionSeconds[timerIndex] = *actionTimers[timerIndex];
        }
        timingBattle.missileAttackTimer_ = 5.0f;
        timingBattle.currentAction_->Update(timingBattle, timingBoss);
        // 子の直接更新では親の待機時間も、他の子の時計も変更しない。
        if (timingBattle.missileAttackTimer_ != 5.0f) {
            return false;
        }
        for (size_t timerIndex = 0; timerIndex < actionTimers.size(); timerIndex++) {
            float expectedSeconds = previousActionSeconds[timerIndex];
            if (timerIndex == actionIndex) {
                expectedSeconds += expectedActionSeconds;
            }
            if (std::abs(*actionTimers[timerIndex] - expectedSeconds) > 0.000001f) {
                return false;
            }
        }
        if (actionType == ActionType::SpiralBarrage
            && Vector3LengthSquared(timingBoss.segments_[0].position - expectedSpiralPosition) > 0.000001f) {
            return false;
        }
        const float previousActiveSeconds = *actionTimers[actionIndex];
        timingBattle.missileAttackTimer_ = 0.0f;
        timingBattle.Update(timingBoss);
        if (actionType == ActionType::HeadCharge) {
            if (timingBattle.actionType_ != ActionType::OrbitShot || timingBattle.orbitShotState_.elapsedSeconds_ != 0.0f) {
                return false;
            }
        } else if (std::abs(*actionTimers[actionIndex] - previousActiveSeconds - expectedActionSeconds) > 0.000001f) {
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

    // 共通移動とビーム追尾を、同じゲーム内1秒で比較する。
    FearWormTestClock fixedClock;
    const std::array<int32_t, 3> movementFrameRates = { 30, 60, 120 };
    std::array<Vector3, 3> aimingDirections {};
    for (size_t rateIndex = 0; rateIndex < movementFrameRates.size(); rateIndex++) {
        const int32_t framesPerSecond = movementFrameRates[rateIndex];
        fixedClock.SetDelta(1.0f / static_cast<float>(framesPerSecond));
        FearWormEnemy movementBoss;
        movementBoss.player_ = &player;
        movementBoss.segments_.resize(1);
        movementBoss.segments_[0].isHead = true;
        movementBoss.segments_[0].hp = movementBoss.kHeadHp;
        const Vector3 movementStart = { 20.0f, 30.0f, 10.0f };
        movementBoss.segments_[0].position = movementStart;
        FearWormBattleState& movementBattle = movementBoss.battleState_;
        // 子の時計を1秒進めても、他の子や親のミサイル待機時間は進まない。
        const std::array<ActionType, 4> localTimerActions = {
            ActionType::OrbitShot, ActionType::WeaveSpread, ActionType::DriftBurst, ActionType::CoilBarrage
        };
        const std::array<float*, 4> localTimers = {
            &movementBattle.orbitShotState_.elapsedSeconds_, &movementBattle.weaveSpreadState_.elapsedSeconds_,
            &movementBattle.driftBurstState_.elapsedSeconds_, &movementBattle.coilBarrageState_.elapsedSeconds_
        };
        for (size_t actionIndex = 0; actionIndex < localTimerActions.size(); actionIndex++) {
            movementBattle.ChangeAction(localTimerActions[actionIndex], movementBoss);
            for (int32_t stepIndex = 0; stepIndex < framesPerSecond; stepIndex++) {
                movementBattle.currentAction_->Update(movementBattle, movementBoss);
            }
            if (std::abs(*localTimers[actionIndex] - 1.0f) > 0.00001f
                || movementBattle.missileAttackTimer_ != 0.0f) {
                return false;
            }
            for (size_t timerIndex = 0; timerIndex < localTimers.size(); timerIndex++) {
                if (timerIndex != actionIndex && *localTimers[timerIndex] != 0.0f) {
                    return false;
                }
            }
            fixedClock.SetDelta(0.0f);
            const float pausedElapsedSeconds = *localTimers[actionIndex];
            movementBattle.currentAction_->Update(movementBattle, movementBoss);
            if (*localTimers[actionIndex] != pausedElapsedSeconds) {
                return false;
            }
            fixedClock.SetDelta(1.0f / static_cast<float>(framesPerSecond));
        }
        // 戦闘更新で1回進め、胴体更新と問い合わせの繰り返しでは時計を進めない。
        movementBoss.segments_.resize(2);
        movementBattle.ChangeAction(ActionType::LineMissile, movementBoss);
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond; stepIndex++) {
            movementBattle.Update(movementBoss);
            movementBattle.GetFormationBlend();
            movementBoss.UpdateSegments();
            movementBoss.UpdateSegments();
        }
        if (std::abs(movementBattle.lineMissileState_.formationElapsedSeconds_ - 1.0f) > 0.00001f) {
            return false;
        }
        fixedClock.SetDelta(0.0f);
        const float pausedFormationSeconds = movementBattle.lineMissileState_.formationElapsedSeconds_;
        movementBattle.Update(movementBoss);
        movementBoss.UpdateSegments();
        if (movementBattle.lineMissileState_.formationElapsedSeconds_ != pausedFormationSeconds) {
            return false;
        }
        fixedClock.SetDelta(1.0f / static_cast<float>(framesPerSecond));
        movementBattle.ChangeAction(ActionType::OrbitShot, movementBoss);
        if (movementBattle.lineMissileState_.formationElapsedSeconds_ != 0.0f) {
            return false;
        }
        // 同じフレームに行動が切り替わっても、整列は従来通りそのフレームから進む。
        movementBattle.missileAttackTimer_ = movementBattle.kMissileAttackInterval;
        movementBattle.orbitShotState_.elapsedSeconds_ = 100.0f;
        movementBattle.Update(movementBoss);
        if (movementBattle.actionType_ != ActionType::LineMissile
            || std::abs(movementBattle.lineMissileState_.formationElapsedSeconds_
                - TimeManager::GetInstance()->GetDeltaTime()) > 0.000001f) {
            return false;
        }
        movementBattle.ChangeAction(ActionType::OrbitShot, movementBoss);
        movementBoss.segments_.resize(1);
        movementBoss.segments_[0].position = movementStart;
        movementBattle.movementPattern_ = FearWormBattleState::MovementPattern::Drift;
        const Vector3 movementTarget = FearWormMovement::CalculateDriftTarget(movementBattle.CreateMovementContext(movementBoss, player.GetTranslate()));
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond; stepIndex++) {
            movementBattle.UpdateBattleMovement(movementBoss, movementTarget, 1.0f);
        }
        const Vector3 expectedMovementPosition = movementTarget
            + (movementStart - movementTarget) * std::pow(1.0f - movementBoss.moveSpeed_, 60.0f);
        if (Vector3LengthSquared(movementBoss.GetPosition() - expectedMovementPosition) > 0.000001f) {
            return false;
        }
        movementBoss.segments_[0].position = { 0.0f, 0.0f, 10.0f };
        movementBattle.headAimDirection_ = { 1.0f, 0.0f, 0.0f };
        FearWormBeamState& aimingState = movementBattle.beamActionState_;
        aimingState.phase_ = FearWormBeamState::Phase::Fire;
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond; stepIndex++) {
            aimingState.UpdateFire(movementBattle, movementBoss, timeManager->GetDeltaTime());
        }
        aimingDirections[rateIndex] = movementBattle.headAimDirection_;
        if (aimingState.beam_.GetLength() != 400.0f) {
            return false;
        }
        fixedClock.SetDelta(0.0f);
        const Vector3 pausedHeadPosition = movementBoss.GetPosition();
        movementBattle.UpdateBattleMovement(movementBoss, movementTarget, 1.0f);
        if (Vector3LengthSquared(movementBoss.GetPosition() - pausedHeadPosition) != 0.0f) {
            return false;
        }
    }
    // 正規化補間の刻み差は約0.6度以内とする。固定係数の追尾ではこの範囲を大きく超える。
    if (Vector3LengthSquared(aimingDirections[0] - aimingDirections[1]) > 0.0001f
        || Vector3LengthSquared(aimingDirections[2] - aimingDirections[1]) > 0.0001f) {
        return false;
    }

    std::puts("FearWorm battle actions: cycle, priorities, cancellation, timing and reset passed.");
    return true;
}
