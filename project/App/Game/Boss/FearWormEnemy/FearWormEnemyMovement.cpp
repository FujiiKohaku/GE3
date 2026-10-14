#include "FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Time/TimeManager.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kFallbackMoveXFrequency = 1.00f;
constexpr float kFallbackMoveXAmplitude = 6.0f;
constexpr float kFallbackMoveYFrequency = 1.30f;
constexpr float kFallbackMoveYAmplitude = 2.5f;
constexpr float kFallbackMoveZFrequency = 0.60f;
constexpr float kFallbackMoveZAmplitude = 3.0f;
constexpr float kEntryOffsetX = -34.0f;
constexpr float kEntryOffsetY = 18.0f;
constexpr float kEntryExtraForwardOffset = 22.0f;
constexpr float kMaximumMovementSpeedAdd = 0.45f;
}

Vector3 FearWormEnemy::FallbackTarget()
{
    Vector3 target = startPosition_;
    target.x += std::sin(moveTime_ * kFallbackMoveXFrequency) * kFallbackMoveXAmplitude;
    target.y += std::sin(moveTime_ * kFallbackMoveYFrequency) * kFallbackMoveYAmplitude;
    target.z += std::sin(moveTime_ * kFallbackMoveZFrequency) * kFallbackMoveZAmplitude;
    return target;
}

Vector3 FearWormEnemy::EntryTarget(const Vector3& playerPosition)
{
    enterTimer_ += TimeManager::GetInstance()->GetDeltaTime();
    float enterRate = enterTimer_ / enterDuration_;
    if (enterRate > 1.0f) {
        enterRate = 1.0f;
    }
    // 登場演出中は、登場開始位置から目標位置まで線形補間する
    return Lerp(EntryStartPosition(playerPosition), MovementTargetPosition(playerPosition), enterRate);
}

Vector3 FearWormEnemy::BattleTarget(const Vector3& playerPosition, float rate)
{
    UpdateMovementPattern(rate);

    Vector3 adjustedPlayerPosition = playerPosition;

    // プレイヤーのカメラのピッチ角（上下の角度）を取得し、基準とするY座標を補正する
    if (player_ != nullptr && player_->GetCamera() != nullptr) {
        Camera* camera = player_->GetCamera();
        
        // カメラのピッチ角を取得（ラジアン）
        // 上を向くと pitch < 0 になるため、符号を反転します
        float pitch = -camera->GetRotate().x;

        // プレイヤーとボスの奥行き（Z方向）の距離
        float distanceZ = std::abs(playerPosition.z - GetPosition().z);

        // プレイヤーから見たカメラの視線の高低差（Yオフセット）を計算
        float lookOffsetY = distanceZ * std::sin(pitch);

        // ボスの基準高度を補正
        adjustedPlayerPosition.y += lookOffsetY;
    }

    return MovementTargetPosition(adjustedPlayerPosition);
}

Vector3 FearWormEnemy::EntryStartPosition(const Vector3& playerPosition) const
{
    // プレイヤーの左上かつ前方を登場開始位置にする。
    Vector3 result = playerPosition;
    result.x += kEntryOffsetX;
    result.y += kEntryOffsetY;
    result.z += parallelForwardOffset_ + kEntryExtraForwardOffset;
    return result;
}

Vector3 FearWormEnemy::MovementTargetPosition(const Vector3& playerPosition) const
{
    const FearWormMovementContext context { playerPosition, orbitAngle_, movementPatternTimer_,
        parallelForwardOffset_, madModeTimer_, spiralAngle_ };
    return CalculateFearWormTarget(movementPattern_, context);
}

void FearWormEnemy::UpdateMovementPattern(float movementSpeedRate)
{
    if (movementPattern_ == MovementPattern::Line || movementPattern_ == MovementPattern::Spiral) return;
    movementPatternTimer_ += TimeManager::GetInstance()->GetDeltaTime() * movementSpeedRate;
    if (movementPatternTimer_ >= movementPatternDuration_) {
        ChangeMovementPattern(NextFearWormMovement(movementPattern_));
    }
}

void FearWormEnemy::ChangeMovementPattern(MovementPattern pattern)
{
    movementPattern_ = pattern;
    if (pattern == MovementPattern::Line) {
        lineTransitionTimer_ = 0.0f;
    } else if (pattern != MovementPattern::Spiral) {
        movementPatternTimer_ = 0.0f;
    }
}

void FearWormEnemy::UpdateSegments()
{
    if (segments_.empty()) {
        return;
    }

    float blendT = 0.0f;
    if (movementPattern_ == MovementPattern::Line) {
        lineTransitionTimer_ += TimeManager::GetInstance()->GetDeltaTime();
        blendT = (std::min)(lineTransitionTimer_, 1.0f);
    } else {
        lineTransitionTimer_ = 0.0f;
    }
    Segment* previousSegment = &segments_[0];
    int32_t aliveOrder = 0;

    for (size_t index = 1; index < segments_.size(); index = index + 1) {
        Segment& segment = segments_[index];

        if (!segment.isAlive) {
            continue;
        }
        aliveOrder++;

        // 1. 通常の追従目標位置 (縦並び - 元の引き戻し拘束)
        Vector3 diff = segment.position - previousSegment->position;
        Vector3 direction = Normalize(diff);
        Vector3 posNormal = previousSegment->position + direction * segmentSpacing_;

        // 2. Line状態の目標位置 (横並び＋うねうね)
        float waveScaleY = 3.2f;
        float waveScaleZ = 2.0f;
        float waveFrequency = 4.2f;

        Vector3 posLine = {
            segments_[0].position.x - (segmentSpacing_ * static_cast<float>(aliveOrder)),
            segments_[0].position.y + std::sin(moveTime_ * waveFrequency + static_cast<float>(aliveOrder) * 0.8f) * waveScaleY,
            segments_[0].position.z + std::cos(moveTime_ * (waveFrequency * 0.8f) + static_cast<float>(aliveOrder) * 0.6f) * waveScaleZ
        };

        // 3. 通常位置とLine位置をブレンドして直接適用する
        // blendT = 0.0f のときは posNormal (従来と100%同一の挙動) になります
        segment.position = Lerp(posNormal, posLine, blendT);

        previousSegment = &segment;
    }
}

float FearWormEnemy::MovementSpeedRate() const
{
    // HPが減るほど移動と攻撃の速度倍率を上げる。
    float healthRate = HealthRate();

    float damageRate = 1.0f - healthRate;

    return 1.0f + damageRate * kMaximumMovementSpeedAdd;
}
