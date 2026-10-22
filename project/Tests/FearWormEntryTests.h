#pragma once
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include <cmath>
#include <cstdio>

inline bool RunFearWormEntryTests()
{
    constexpr size_t kSegmentCount = 10;
    constexpr float kSpacing = 5.0f;
    const Vector3 battlePosition = { -11.2f, 12.0f, 5200.0f };
    const float durationSeconds = FearWormEntryState::CalculateEntryDurationSeconds(kSegmentCount);
    for (float waterHeight : { -30.0f, 15.0f }) {
        const Vector3 surfacePosition = { -12.0f, waterHeight, 5234.0f };
        for (size_t index = 0; index < kSegmentCount; ++index) {
            const Vector3 waitingPosition = FearWormEntryState::CalculateEntryPosition(surfacePosition,
                battlePosition, FearWormEntryState::kEntryShadowDurationSeconds, index, kSpacing);
            if (waitingPosition.y >= waterHeight) { return false; }
            const Vector3 finalPosition = FearWormEntryState::CalculateEntryPosition(surfacePosition,
                battlePosition, durationSeconds, index, kSpacing);
            if (std::abs(finalPosition.x - battlePosition.x) > 0.001f
                || std::abs(finalPosition.y - battlePosition.y) > 0.001f
                || std::abs(finalPosition.z - (battlePosition.z - index * kSpacing)) > 0.001f) { return false; }
        }
        // 低フレームレートでも、頭から順番に水面を越えることを確認する。
        for (float deltaSeconds : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f }) {
            float previousCrossingSeconds = -1.0f;
            for (size_t index = 0; index < kSegmentCount; ++index) {
                bool hasCrossedSurface = false;
                Vector3 previousPosition = FearWormEntryState::CalculateEntryPosition(surfacePosition, battlePosition, 0.0f, index, kSpacing);
                for (float elapsedSeconds = deltaSeconds; elapsedSeconds < durationSeconds; elapsedSeconds += deltaSeconds) {
                    const Vector3 position = FearWormEntryState::CalculateEntryPosition(surfacePosition, battlePosition, elapsedSeconds, index, kSpacing);
                    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)
                        || std::abs(position.x - previousPosition.x) > 8.0f
                        || std::abs(position.y - previousPosition.y) > 8.0f
                        || std::abs(position.z - previousPosition.z) > 8.0f) { return false; }
                    if (!hasCrossedSurface && position.y >= waterHeight) {
                        if (elapsedSeconds <= previousCrossingSeconds) { return false; }
                        previousCrossingSeconds = elapsedSeconds;
                        hasCrossedSurface = true;
                    }
                    previousPosition = position;
                }
                if (!hasCrossedSurface) { return false; }
            }
        }
    }
    // 実際の内部ステートを通し、各段階でも全身の軌道が変わらないことを確認する。
    using EntryPhase = FearWormEntryState::EntryPhase;
    // 親のUpdateを呼ばなくても、各ステート自身が位置と演出を更新する。
    {
        FearWormEnemy boss;
        Player player;
        boss.player_ = &player;
        boss.segments_.resize(kSegmentCount);
        // 登場を経由しない戦闘開始では、既存の開始角度を保持する。
        boss.battleState_.orbitAngle_ = 0.4f;
        boss.ChangeState(FearWormEnemy::BossState::Battle);
        if (boss.battleState_.orbitAngle_ != 0.4f) { return false; }
        boss.ChangeState(FearWormEnemy::BossState::Wait);
        boss.battleState_.movementPattern_ = FearWormBattleState::MovementPattern::Line;
        boss.battleState_.missileAttackTimer_ = 9.0f;
        boss.ChangeState(FearWormEnemy::BossState::Entry);
        // 登場開始は兄弟Stateの角度・パターン・待機時間を準備しない。
        if (boss.battleState_.orbitAngle_ != 0.4f
            || boss.battleState_.movementPattern_ != FearWormBattleState::MovementPattern::Line
            || boss.battleState_.missileAttackTimer_ != 9.0f
            || boss.battleState_.currentAction_ != nullptr) { return false; }
        // 同じJumOutで、予兆中と飛び出し中の両方を更新する。
        for (float elapsedSeconds : { 1.0f, 3.0f, 4.5f }) {
            EntryPhase phase = EntryPhase::JumOut;
            if (elapsedSeconds >= 4.4f) {
                phase = EntryPhase::WaitForBody;
            }
            boss.entryState_.ChangePhase(phase, boss);
            boss.entryState_.elapsedSeconds_ = elapsedSeconds;
            for (auto& segment : boss.segments_) {
                segment.position = { -999.0f, -999.0f, -999.0f };
            }
            boss.entryState_.currentPhase_->Update(boss.entryState_, boss);
            if (boss.entryState_.elapsedSeconds_ != elapsedSeconds) { return false; }
            const Vector3 surfacePosition = boss.entryState_.EntrySurfacePosition(player.GetTranslate());
            const FearWormMovementContext movementContext { player.GetTranslate(), 2.35f, boss.parallelForwardOffset_ };
            const Vector3 targetPosition = FearWormMovement::CalculateOrbitTarget(movementContext);
            if (boss.battleState_.orbitAngle_ != 0.4f
                || boss.battleState_.movementPattern_ != FearWormBattleState::MovementPattern::Line
                || boss.battleState_.missileAttackTimer_ != 9.0f) { return false; }
            for (size_t index = 0; index < kSegmentCount; ++index) {
                const Vector3 expectedPosition = FearWormEntryState::CalculateEntryPosition(
                    surfacePosition, targetPosition, elapsedSeconds, index, boss.segmentSpacing_);
                const Vector3 actualPosition = boss.segments_[index].position;
                if (std::abs(actualPosition.x - expectedPosition.x) > 0.001f
                    || std::abs(actualPosition.y - expectedPosition.y) > 0.001f
                    || std::abs(actualPosition.z - expectedPosition.z) > 0.001f) { return false; }
            }
            if (elapsedSeconds == 1.0f && boss.HasBreachedSurface()) { return false; }
            if (elapsedSeconds == 3.0f && !boss.HasBreachedSurface()) { return false; }
        }
    }
    for (size_t segmentCount : { size_t{1}, size_t{10} }) {
        for (float deltaSeconds : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f }) {
            FearWormEnemy boss;
            Player player;
            boss.player_ = &player;
            boss.segments_.resize(segmentCount);
            boss.ChangeState(FearWormEnemy::BossState::Entry);
            if (boss.entryState_.GetPhase() != EntryPhase::JumOut) { return false; }

            const float entryDurationSeconds = FearWormEntryState::CalculateEntryDurationSeconds(segmentCount);
            const Vector3 surfacePosition = boss.entryState_.EntrySurfacePosition(player.GetTranslate());
            const FearWormMovementContext movementContext { player.GetTranslate(), 2.35f, boss.parallelForwardOffset_ };
            const Vector3 targetPosition = FearWormMovement::CalculateOrbitTarget(movementContext);
            for (float elapsedSeconds = 0.0f; elapsedSeconds < entryDurationSeconds; elapsedSeconds += deltaSeconds) {
                boss.entryState_.elapsedSeconds_ = elapsedSeconds;
                boss.UpdateState();
                if (boss.GetBossState() != FearWormEnemy::BossState::Entry) { return false; }
                EntryPhase expectedPhase = EntryPhase::JumOut;
                if (elapsedSeconds >= FearWormEntryState::kEntryShadowDurationSeconds
                    + FearWormEntryState::kEntryFlightDurationSeconds) {
                    expectedPhase = EntryPhase::WaitForBody;
                }
                if (boss.entryState_.GetPhase() != expectedPhase) { return false; }
                for (size_t index = 0; index < segmentCount; ++index) {
                    const Vector3 expectedPosition = FearWormEntryState::CalculateEntryPosition(
                        surfacePosition, targetPosition, elapsedSeconds, index, boss.segmentSpacing_);
                    const Vector3 actualPosition = boss.segments_[index].position;
                    if (std::abs(actualPosition.x - expectedPosition.x) > 0.001f
                        || std::abs(actualPosition.y - expectedPosition.y) > 0.001f
                        || std::abs(actualPosition.z - expectedPosition.z) > 0.001f) { return false; }
                }
            }
            boss.entryState_.elapsedSeconds_ = entryDurationSeconds;
            boss.UpdateState();
            if (boss.GetBossState() != FearWormEnemy::BossState::Battle
                || boss.entryState_.currentPhase_ != nullptr || !boss.HasBreachedSurface()) { return false; }
            if (boss.battleState_.orbitAngle_ != 2.35f
                || boss.battleState_.currentAction_ != &boss.battleState_.orbitShotState_
                || Vector3LengthSquared(boss.GetPosition() - targetPosition) > 0.000001f) { return false; }

            // 再登場で予兆からやり直し、大きな時間飛びでも同じフレームで戦闘へ移る。
            boss.ChangeState(FearWormEnemy::BossState::Entry);
            if (boss.entryState_.GetPhase() != EntryPhase::JumOut
                || boss.entryState_.elapsedSeconds_ != 0.0f || boss.HasBreachedSurface()) { return false; }
            boss.entryState_.elapsedSeconds_ = entryDurationSeconds + 1.0f;
            boss.UpdateState();
            if (boss.GetBossState() != FearWormEnemy::BossState::Battle
                || !boss.HasBreachedSurface() || boss.battleState_.orbitAngle_ != 2.35f) { return false; }

            // 登場の中断で内部ステートを終了し、水位設定を保ったまま配置し直す。
            boss.ChangeState(FearWormEnemy::BossState::Entry);
            boss.SetEntryWaterHeight(15.0f);
            boss.SetPosition({ 0.0f, 0.0f, 0.0f });
            if (boss.GetBossState() != FearWormEnemy::BossState::Wait
                || boss.entryState_.currentPhase_ != nullptr
                || boss.entryState_.elapsedSeconds_ != 0.0f
                || boss.entryState_.GetWaterHeight() != 15.0f) { return false; }
        }
    }
    std::puts("FearWorm entry: nested phases, continuous motion, reset and battle handoff passed.");
    return true;
}
