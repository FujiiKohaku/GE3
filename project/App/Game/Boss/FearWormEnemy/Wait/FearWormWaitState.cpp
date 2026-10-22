#include "FearWormWaitState.h"
#include "../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include <cmath>
#include "../Battle/State/FearWormTiming.h"
#include "Engine/Time/TimeManager.h"

void FearWormWaitState::Update(FearWormEnemy& boss)
{
    float movementSpeedRate = boss.MovementSpeedRate();
    Vector3 target;

    if (boss.player_ == nullptr) {
        target = CalculateFallbackTarget(startPosition_, boss.moveTime_);
    } else {
        Vector3 playerPosition = boss.player_->GetTranslate();

        // 起動条件チェック
        if (playerPosition.z >= startPosition_.z - kActivationLeadDistance) {
            boss.ChangeState(FearWormEnemy::BossState::Entry); // 登場状態へ移行
            return;
        }

        target = CalculateWaitingPosition(playerPosition, boss.parallelForwardOffset_);
    }

    // 移動処理
    const float movementBlend = FearWormTiming::CalculateFollowBlend(boss.moveSpeed_ * movementSpeedRate,
        TimeManager::GetInstance()->GetDeltaTime());
    boss.segments_[0].position = Lerp(boss.segments_[0].position, target, movementBlend);
    boss.transform_.translate = boss.segments_[0].position;
}

Vector3 FearWormWaitState::CalculateWaitingPosition(const Vector3& playerPosition, float forwardOffset)
{
    Vector3 result = playerPosition;
    result.x += kWaitingOffsetX;
    result.y += kWaitingOffsetY;
    result.z += forwardOffset + kWaitingExtraForwardOffset;
    return result;
}

Vector3 FearWormWaitState::CalculateFallbackTarget(const Vector3& startPosition, float elapsedSeconds)
{
    Vector3 target = startPosition;
    target.x += std::sin(elapsedSeconds * kFallbackMoveXFrequency) * kFallbackMoveXAmplitude;
    target.y += std::sin(elapsedSeconds * kFallbackMoveYFrequency) * kFallbackMoveYAmplitude;
    target.z += std::sin(elapsedSeconds * kFallbackMoveZFrequency) * kFallbackMoveZAmplitude;
    return target;
}
