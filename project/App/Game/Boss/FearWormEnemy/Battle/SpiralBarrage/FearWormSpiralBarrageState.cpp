#include "FearWormSpiralBarrageState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include "../State/FearWormTiming.h"
#include <algorithm>

void FearWormSpiralBarrageState::Reset()
{
    isFinished_ = false;
    elapsedSeconds_ = 0.0f;
    spiralAngle_ = 0.0f;
    barrageAngle_ = 0.0f;
    fireElapsedSeconds_ = 0.0f;
}

void FearWormSpiralBarrageState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    Reset();
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Spiral;
}

void FearWormSpiralBarrageState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    elapsedSeconds_ += deltaTimeSeconds;
    spiralAngle_ += deltaTimeSeconds * 2.5f * boss.MovementSpeedRate();
    barrageAngle_ += deltaTimeSeconds * 4.0f;
    const FearWormMovementContext context = battle.CreateMovementContext(boss, boss.player_->GetTranslate());
    battle.UpdateBattleMovement(boss, CalculateTarget(context, elapsedSeconds_, spiralAngle_), boss.MovementSpeedRate());
    if (battle.CanFire(boss)) {
        FireBarrage(battle, boss);
    }
    isFinished_ = elapsedSeconds_ >= kThreatDurationSeconds + kSpiralDurationSeconds;
}

void FearWormSpiralBarrageState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    fireElapsedSeconds_ = 0.0f;
}

void FearWormSpiralBarrageState::FireBarrage(FearWormBattleState& battle, FearWormEnemy& boss)
{
    // 最初の2秒間（威嚇中）は弾を撃たずにプレイヤーに猶予を与える
    if (elapsedSeconds_ < kThreatDurationSeconds) {
        return;
    }

    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    const float activeDeltaSeconds = (std::min)(deltaTimeSeconds, (std::max)(0.0f, elapsedSeconds_ - kThreatDurationSeconds));
    const int32_t shotCount = FearWormTiming::AdvanceInterval(fireElapsedSeconds_,
        kFireIntervalSeconds, activeDeltaSeconds, kMaxShotsPerUpdate);
    if (shotCount == 0) {
        return;
    }

    Vector3 headPos = boss.GetPosition();
    Vector3 playerPos = boss.player_->GetTranslate();

    // ボスからプレイヤーへの方向ベクトルを基準にする（プレイヤー狙い補正）
    Vector3 toPlayer = NormalizeSafe(playerPos - headPos);

    // toPlayer に直交する直交座標系 (right, up) を構築
    Vector3 tempUp = { 0.0f, 1.0f, 0.0f };
    if (std::abs(toPlayer.y) > 0.99f) {
        tempUp = { 1.0f, 0.0f, 0.0f };
    }
    Vector3 right = NormalizeSafe(Cross(tempUp, toPlayer));
    Vector3 up = NormalizeSafe(Cross(toPlayer, right));

    float spread = (std::sin(barrageAngle_ * 2.5f) * 0.22f) + 0.22f; // 0.0(正確に狙う)〜0.44(周囲に拡散)を変動させる

    // プレイヤー方向を軸とした直交平面上で対角線上に広がる2つの螺旋オフセットベクトル
    Vector3 offset1 = right * (std::cos(barrageAngle_) * spread) + up * (std::sin(barrageAngle_) * spread);
    Vector3 offset2 = right * (std::cos(barrageAngle_ + battle.kPi) * spread) + up * (std::sin(barrageAngle_ + battle.kPi) * spread);

    // プレイヤー方向ベクトルと螺旋オフセットを合成して最終的な発射方向を決定
    Vector3 dir1 = NormalizeSafe(toPlayer + offset1);
    Vector3 dir2 = NormalizeSafe(toPlayer + offset2);

    for (int32_t shotIndex = 0; shotIndex < shotCount; shotIndex++) {
        battle.FireDirectionalBullet(boss, headPos, dir1);
        battle.FireDirectionalBullet(boss, headPos, dir2);
    }
}

Vector3 FearWormSpiralBarrageState::CalculateTarget(const FearWormMovementContext& context,
    float elapsedSeconds, float spiralAngleRadians)
{
    Vector3 result = context.playerPosition;
    const float forwardDistance = context.forwardOffset + kForwardOffsetAdd;

    // 最初の2秒間は画面中央に静止して威嚇
    if (elapsedSeconds < kThreatDurationSeconds) {
        result.x += 0.0f;
        result.y += kHeight;
        result.z += forwardDistance;
    } else {
        // 後半の5秒間で、半径を0から最大値まで広げる。
        const float spiralRate = (elapsedSeconds - kThreatDurationSeconds) / kSpiralDurationSeconds;
        const float spiralRadius = spiralRate * kMaximumRadius;

        result.x += std::cos(spiralAngleRadians) * spiralRadius;
        result.y += kHeight + std::sin(spiralAngleRadians) * spiralRadius;
        result.z += forwardDistance;
    }

    // 地面にめり込まないように下限（Y = kMinimumHeight）をかける
    if (result.y < kMinimumHeight) {
        result.y = kMinimumHeight;
    }

    return result;
}
