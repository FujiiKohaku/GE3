#pragma once
#include "App/Game/Boss/FearWormEnemy/FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include <cmath>

inline bool RunBeamStateTests()
{
    // 描画初期化なしで、線分の途中・端・外側と無効な寸法を検証する。
    FearWormBeam beam;
    beam.SetDimensions(10.0f, 2.0f);
    const Vector3 beamPosition = { 2.0f, 3.0f, 4.0f };
    const Vector3 beamDirection = { 0.0f, 0.0f, -1.0f };
    beam.SetPose(beamPosition, beamDirection);
    if (!beam.IntersectsSphere({ 2.0f, 3.0f, -1.0f }, 1.0f)
        || !beam.IntersectsSphere({ 4.0f, 3.0f, -1.0f }, 1.0f)
        || beam.IntersectsSphere({ 4.01f, 3.0f, -1.0f }, 1.0f)
        || !beam.IntersectsSphere({ 2.0f, 3.0f, -8.0f }, 1.0f)
        || beam.IntersectsSphere({ 2.0f, 3.0f, -8.01f }, 1.0f)
        || beam.IntersectsSphere({ 2.0f, 3.0f, 6.01f }, 1.0f)) {
        return false;
    }
    // 姿勢を変更すると、前の軌道ではなく保持した新しい軌道で判定する。
    beam.SetPose(beamPosition, { 1.0f, 0.0f, 0.0f });
    if (!beam.IntersectsSphere({ 7.0f, 3.0f, 4.0f }, 1.0f)
        || beam.IntersectsSphere({ 2.0f, 3.0f, -1.0f }, 1.0f)) {
        return false;
    }
    beam.SetPose({ 20.0f, 3.0f, 4.0f }, { 1.0f, 0.0f, 0.0f });
    if (!beam.IntersectsSphere({ 25.0f, 3.0f, 4.0f }, 1.0f)
        || beam.IntersectsSphere({ 7.0f, 3.0f, 4.0f }, 1.0f)) {
        return false;
    }
    beam.SetDimensions(0.0f, 2.0f);
    if (beam.IntersectsSphere(beamPosition, 1.0f)) {
        return false;
    }
    beam.SetDimensions(10.0f, 0.0f);
    if (beam.IntersectsSphere(beamPosition, 1.0f)) {
        return false;
    }
    beam.Reset();
    beam.UpdateVisual(0.1f);
    beam.CheckCollision(nullptr);
    if (beam.GetLength() != 0.0f || beam.GetWidth() != 0.0f || beam.IsHittingPlayer()
        || Vector3LengthSquared(beam.GetPosition()) != 0.0f
        || beam.GetDirection().z != -1.0f) {
        return false;
    }

    FearWormEnemy boss;
    Player player;
    boss.player_ = &player;
    boss.segments_.resize(1);
    boss.segments_[0].position = { 0.0f, 0.0f, 10.0f };
    boss.battleState_.beamActionState_.ChangePhase(boss.battleState_, boss, FearWormBeamState::Phase::Wait);
    boss.battleState_.beamActionState_.elapsedSeconds_ = 3.99f;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.01f);
    if (boss.battleState_.beamActionState_.phase_ != FearWormBeamState::Phase::Wait) return false;
    boss.battleState_.beamActionState_.elapsedSeconds_ = 4.0f;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.01f);
    if (boss.battleState_.beamActionState_.phase_ != FearWormBeamState::Phase::Charge || boss.battleState_.beamActionState_.elapsedSeconds_ != 0.0f) return false;
    boss.battleState_.beamActionState_.elapsedSeconds_ = 1.49f;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.01f);
    if (boss.battleState_.beamActionState_.phase_ != FearWormBeamState::Phase::Charge || boss.battleState_.beamActionState_.beam_.GetWidth() != 0.0f) return false;
    boss.battleState_.beamActionState_.elapsedSeconds_ = 1.5f;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.01f);
    if (boss.battleState_.beamActionState_.phase_ != FearWormBeamState::Phase::Fire || boss.battleState_.beamActionState_.elapsedSeconds_ != 0.0f) return false;
    boss.battleState_.beamActionState_.elapsedSeconds_ = 2.99f;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.1f);
    if (boss.battleState_.beamActionState_.phase_ != FearWormBeamState::Phase::Fire || boss.battleState_.beamActionState_.beam_.GetLength() != 100.0f ||
        boss.battleState_.beamActionState_.beam_.GetWidth() != 3.5f) return false;
    boss.battleState_.beamActionState_.elapsedSeconds_ = 3.0f;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.5f);
    if (boss.battleState_.beamActionState_.phase_ != FearWormBeamState::Phase::FadeOut || boss.battleState_.beamActionState_.elapsedSeconds_ != 0.0f ||
        boss.battleState_.beamActionState_.beam_.GetLength() != 400.0f) return false;
    const int hp = player.GetCurrentHp();
    boss.battleState_.beamActionState_.elapsedSeconds_ = 0.25f;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.25f);
    if (std::abs(boss.battleState_.beamActionState_.beam_.GetWidth() - 1.5f) > 0.0001f || player.GetCurrentHp() != hp) return false;
    boss.battleState_.beamActionState_.elapsedSeconds_ = 0.5f;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.25f);
    if (boss.battleState_.beamActionState_.phase_ != FearWormBeamState::Phase::Wait || boss.battleState_.beamActionState_.elapsedSeconds_ != 0.0f) return false;
    boss.battleState_.beamActionState_.UpdatePhase(boss.battleState_, boss, 0.01f);
    if (boss.battleState_.beamActionState_.beam_.GetWidth() != 0.0f || boss.battleState_.beamActionState_.beam_.GetLength() != 0.0f) return false;
    boss.battleState_.beamActionState_.elapsedSeconds_ = 2.0f;
    boss.battleState_.beamActionState_.ChangePhase(boss.battleState_, boss, FearWormBeamState::Phase::Wait);
    if (boss.battleState_.beamActionState_.elapsedSeconds_ != 2.0f) {
        return false; // 同じ段階を指定しても再初期化しない。
    }

    // 親から内部値を書き戻さなくても、終了処理だけで待機状態へ戻る。
    FearWormBeamState& beamState = boss.battleState_.beamActionState_;
    beamState.phase_ = FearWormBeamState::Phase::Fire;
    beamState.elapsedSeconds_ = 2.0f;
    beamState.effectTimerSeconds_ = 0.15f;
    beamState.pendingShotEffectCount_++;
    beamState.beam_.SetDimensions(100.0f, 3.5f);
    beamState.Exit(boss.battleState_, boss);
    if (beamState.IsAiming() || beamState.IsHittingPlayer()
        || beamState.elapsedSeconds_ != 0.0f || beamState.effectTimerSeconds_ != 0.0f
        || beamState.pendingShotEffectCount_ != 0
        || beamState.beam_.GetLength() != 0.0f || beamState.beam_.GetWidth() != 0.0f) {
        return false;
    }
    beamState.Stop();
    if (beamState.IsAiming() || beamState.IsHittingPlayer()) {
        return false;
    }

    // ビームの各段階でX・Y固定とZ追従を確認し、待機・終了後は通常移動へ戻す。
    boss.battleState_.movementPattern_ = FearWormBattleState::MovementPattern::Drift;
    const Vector3 startPosition = { 20.0f, 30.0f, 10.0f };
    const Vector3 movementTarget = boss.battleState_.BattleTarget(boss, player.GetTranslate());
    const Vector3 expectedMovement = Lerp(startPosition, movementTarget,
        boss.moveSpeed_ * boss.MovementSpeedRate());
    if (std::abs(expectedMovement.x - startPosition.x) < 0.0001f
        || std::abs(expectedMovement.y - startPosition.y) < 0.0001f
        || std::abs(expectedMovement.z - startPosition.z) < 0.0001f) {
        return false;
    }
    const FearWormBeamState::Phase aimingPhases[] = {
        FearWormBeamState::Phase::Charge,
        FearWormBeamState::Phase::Fire,
        FearWormBeamState::Phase::FadeOut
    };
    for (const FearWormBeamState::Phase phase : aimingPhases) {
        boss.segments_[0].position = startPosition;
        beamState.phase_ = phase;
        beamState.UpdateMovement(boss.battleState_, boss);
        const Vector3 headPosition = boss.segments_[0].position;
        if (headPosition.x != startPosition.x || headPosition.y != startPosition.y
            || std::abs(headPosition.z - expectedMovement.z) > 0.0001f
            || Vector3LengthSquared(boss.transform_.translate - headPosition) > 0.000001f) {
            return false;
        }
    }
    beamState.Stop();
    boss.segments_[0].position = startPosition;
    beamState.UpdateMovement(boss.battleState_, boss);
    if (Vector3LengthSquared(boss.segments_[0].position - expectedMovement) > 0.000001f) {
        return false;
    }
    // 共通移動は、ビーム側に照準状態が残っていても制限されない。
    beamState.phase_ = FearWormBeamState::Phase::Charge;
    boss.segments_[0].position = startPosition;
    boss.battleState_.UpdateBattleMovement(boss, player.GetTranslate(), boss.MovementSpeedRate());
    if (Vector3LengthSquared(boss.segments_[0].position - expectedMovement) > 0.000001f) {
        return false;
    }
    beamState.Stop();

    // StateのUpdateが、照準更新後の発射口と方向を一度だけBeamへ渡す。
    boss.battleState_.headAimDirection_ = { 1.0f, 0.0f, 0.0f };
    const Vector3 previousMuzzlePosition = boss.battleState_.HeadMuzzlePosition(boss);
    beamState.phase_ = FearWormBeamState::Phase::Fire;
    beamState.beam_.SetDimensions(100.0f, 3.5f);
    beamState.Update(boss.battleState_, boss);
    const Vector3 expectedPosition = boss.battleState_.HeadMuzzlePosition(boss);
    return Vector3LengthSquared(beamState.beam_.GetPosition() - expectedPosition) < 0.000001f
        && Vector3LengthSquared(beamState.beam_.GetDirection() - boss.battleState_.headAimDirection_) < 0.000001f
        && Vector3LengthSquared(beamState.beam_.GetPosition() - previousMuzzlePosition) > 0.000001f;
}
