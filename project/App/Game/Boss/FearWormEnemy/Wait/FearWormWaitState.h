#pragma once

#include "../State/FearWormState.h"
#include "Engine/Math/MathStruct.h"

class FearWormWaitState final : public FearWormState {
public:
    void SetStartPosition(const Vector3& position) { startPosition_ = position; }
    void Update(FearWormEnemy& boss) override;

private:
    static constexpr float kActivationLeadDistance = 160.0f;
    static constexpr float kFallbackMoveXFrequency = 1.00f;
    static constexpr float kFallbackMoveXAmplitude = 6.0f;
    static constexpr float kFallbackMoveYFrequency = 1.30f;
    static constexpr float kFallbackMoveYAmplitude = 2.5f;
    static constexpr float kFallbackMoveZFrequency = 0.60f;
    static constexpr float kFallbackMoveZAmplitude = 3.0f;

    static constexpr float kWaitingOffsetX = -34.0f;
    static constexpr float kWaitingOffsetY = 18.0f;
    static constexpr float kWaitingExtraForwardOffset = 22.0f;

    // プレイヤーの左上かつ前方に、待機中の目標位置を求める。
    static Vector3 CalculateWaitingPosition(const Vector3& playerPosition, float forwardOffset);
    // プレイヤー不在時は、配置位置を基準に緩やかに動く待機先を求める。
    static Vector3 CalculateFallbackTarget(const Vector3& startPosition, float elapsedSeconds);

    // 待機先の計算と登場開始距離の判定に使う配置基準。
    Vector3 startPosition_ {};
};
