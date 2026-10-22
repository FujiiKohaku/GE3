#pragma once

#include "App/Game/Boss/IceJellyfish/IceJellyfish.h"
#include "App/Game/Player/Player.h"
#include "FearWormTimingTests.h"
#include <array>
#include <cmath>
#include <cstdio>

inline bool ReportIceJellyfishStateTestFailure(int32_t line)
{
    std::printf("IceJellyfish state assertion failed at line %d\n", line);
    return false;
}

inline bool RunIceJellyfishStateTests()
{
    using BossState = IceJellyfish::BossState;
    FearWormTestClock testClock;
    testClock.SetDelta(0.0f);
    IceJellyfish boss;
    IceJellyfish otherBoss;
    if (boss.GetBossState() != BossState::Wait || boss.currentState_ != &boss.waitState_
        || boss.currentState_ == otherBoss.currentState_) {
        return ReportIceJellyfishStateTestFailure(__LINE__);
    }
    boss.waitState_.Update(boss);
    if (boss.GetBossState() != BossState::Entry || boss.currentState_ != &boss.entryState_) {
        return ReportIceJellyfishStateTestFailure(__LINE__);
    }
    boss.entryState_.Update(boss);
    if (boss.GetBossState() != BossState::Battle || boss.currentState_ != &boss.battleState_) {
        return ReportIceJellyfishStateTestFailure(__LINE__);
    }
    boss.battleState_.nextAttackIndex_ = 2;
    boss.ChangeState(BossState::Battle);
    if (boss.battleState_.nextAttackIndex_ != 2) {
        return ReportIceJellyfishStateTestFailure(__LINE__);
    }

    const std::array<int32_t, 3> frameRates = { 30, 60, 120 };
    for (const int32_t framesPerSecond : frameRates) {
        testClock.SetDelta(1.0f / static_cast<float>(framesPerSecond));
        const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
        IceJellyfish movingBoss;
        Player player;
        player.SetTranslate({ 4.0f, 5.0f, 100.0f });
        movingBoss.player_ = &player;
        movingBoss.SetPosition({ 10.0f, 20.0f, 30.0f });
        movingBoss.pulseBarrageWave_ = 1;
        movingBoss.iceSpearVolley_ = 0;
        movingBoss.iceSpearTentacles_.fill(-1);
        movingBoss.icePillarWave_ = 0;
        movingBoss.icePillarPositions_.fill({ 0.0f, 0.0f, 290.0f });
        movingBoss.Update();
        const Vector3 expectedBodyPosition {
            10.0f + std::sin(deltaTimeSeconds * 0.48f) * 8.0f,
            20.0f + std::sin(deltaTimeSeconds * 1.10f) * 5.0f,
            100.0f + movingBoss.kHoverDistance
        };
        // 空の状態を通過しても、浮遊と3攻撃の時計を同じフレームに1回だけ更新する。
        if (movingBoss.GetBossState() != BossState::Battle
            || std::abs(movingBoss.animationTime_ - deltaTimeSeconds) > 0.000001f
            || std::abs(movingBoss.pulseBarrageTimer_ - deltaTimeSeconds) > 0.000001f
            || std::abs(movingBoss.iceSpearTimer_ - deltaTimeSeconds) > 0.000001f
            || std::abs(movingBoss.icePillarTimer_ - deltaTimeSeconds) > 0.000001f
            || Vector3LengthSquared(movingBoss.GetPosition() - expectedBodyPosition) > 0.000001f) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
        testClock.SetDelta(0.0f);
        movingBoss.Update();
        if (movingBoss.animationTime_ != deltaTimeSeconds || movingBoss.iceSpearTimer_ != deltaTimeSeconds
            || movingBoss.icePillarTimer_ != deltaTimeSeconds) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }

        const Vector3 deathStartPosition = movingBoss.bodyPosition_;
        movingBoss.SetDead(true);
        if (movingBoss.GetBossState() != BossState::Dead || movingBoss.IsDeathSequenceFinished()) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
        testClock.SetDelta(deltaTimeSeconds);
        for (int32_t stepIndex = 0; stepIndex < framesPerSecond; stepIndex++) {
            movingBoss.Update();
        }
        const Vector3 expectedDeathPosition = deathStartPosition + Vector3 { 0.0f, -9.0f, 5.0f };
        if (std::abs(movingBoss.deadState_.elapsedSeconds_ - 1.0f) > 0.00001f
            || Vector3LengthSquared(movingBoss.bodyPosition_ - expectedDeathPosition) > 0.0001f
            || movingBoss.iceSpearTimer_ != deltaTimeSeconds || movingBoss.icePillarTimer_ != deltaTimeSeconds
            || movingBoss.IsDeathSequenceFinished()) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
        const float elapsedBeforeNotification = movingBoss.deadState_.elapsedSeconds_;
        movingBoss.SetDead(true);
        movingBoss.OnDeath();
        if (movingBoss.deadState_.elapsedSeconds_ != elapsedBeforeNotification) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
        testClock.SetDelta(0.0f);
        movingBoss.Update();
        if (movingBoss.deadState_.elapsedSeconds_ != elapsedBeforeNotification) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
        testClock.SetDelta(deltaTimeSeconds);
        for (int32_t stepIndex = 0; stepIndex <= framesPerSecond; stepIndex++) {
            movingBoss.Update();
        }
        if (!movingBoss.IsDeathSequenceFinished()) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
        otherBoss.deadState_.elapsedSeconds_ = 0.75f;
        movingBoss.SetDead(false);
        movingBoss.battleState_.nextAttackIndex_ = 2;
        movingBoss.battleState_.isPulseAttackArmed_ = true;
        movingBoss.ResetStates();
        if (movingBoss.GetBossState() != BossState::Wait || movingBoss.IsDeathSequenceFinished()
            || movingBoss.deadState_.elapsedSeconds_ != 0.0f
            || movingBoss.battleState_.nextAttackIndex_ != 0 || movingBoss.battleState_.isPulseAttackArmed_
            || otherBoss.deadState_.elapsedSeconds_ != 0.75f) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
    }
    // 待機・登場中に撃破されても、戦闘を挟まず死亡へ移行する。
    // テスト時計の倍率上限内で、2秒ちょうどの完了境界を検証する。
    testClock.SetDelta(0.0625f);
    for (const BossState initialState : { BossState::Wait, BossState::Entry }) {
        IceJellyfish dyingBoss;
        dyingBoss.ChangeState(initialState);
        dyingBoss.SetDead(true);
        dyingBoss.Update();
        if (dyingBoss.GetBossState() != BossState::Dead || dyingBoss.deadState_.elapsedSeconds_ != 0.0625f) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
        for (int32_t stepIndex = 1; stepIndex < 31; stepIndex++) {
            dyingBoss.Update();
        }
        if (dyingBoss.IsDeathSequenceFinished()) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
        dyingBoss.Update();
        if (!dyingBoss.IsDeathSequenceFinished()) {
            return ReportIceJellyfishStateTestFailure(__LINE__);
        }
    }
    std::puts("IceJellyfish states: first-frame battle, parallel attacks, 30/60/120 FPS, pause, death and reset passed.");
    return true;
}
