#include "FearWormSpiralBarrageState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include <algorithm>

void FearWormSpiralBarrageState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    isFinished_ = false;
    elapsedSeconds_ = 0.0f;
    spiralAngle_ = 0.0f;
    barrageAngle_ = 0.0f;
    fireTimer_ = 0;
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Spiral;
    battle.hasTriggeredMadMode_ = true;
}

void FearWormSpiralBarrageState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    battle.movementPatternTimer_ += deltaTimeSeconds;
    elapsedSeconds_ += deltaTimeSeconds;
    spiralAngle_ += deltaTimeSeconds * 2.5f * boss.MovementSpeedRate();
    barrageAngle_ += deltaTimeSeconds * 4.0f;
    battle.UpdateBattleMovement(boss, boss.player_->GetTranslate(), boss.MovementSpeedRate(),
        elapsedSeconds_, spiralAngle_);
    if (battle.CanFire(boss)) {
        FireBarrage(battle, boss);
    }
    isFinished_ = elapsedSeconds_ >= 7.0f;
}

void FearWormSpiralBarrageState::Exit(FearWormBattleState& battle, FearWormEnemy&)
{
    fireTimer_ = 0;
    battle.missileAttackTimer_ = 0.0f;
}

void FearWormSpiralBarrageState::FireBarrage(FearWormBattleState& battle, FearWormEnemy& boss)
{
    // 最初の2秒間（威嚇中）は弾を撃たずにプレイヤーに猶予を与える
    if (elapsedSeconds_ < 2.0f) {
        return;
    }

    fireTimer_++;
    if (fireTimer_ < 3) {
        return;
    }
    fireTimer_ = 0;

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

    battle.FireDirectionalBullet(boss, headPos, dir1);
    battle.FireDirectionalBullet(boss, headPos, dir2);
}
