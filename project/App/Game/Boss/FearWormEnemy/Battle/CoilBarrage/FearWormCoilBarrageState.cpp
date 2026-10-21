#include "FearWormCoilBarrageState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include <cmath>
#include <algorithm>

void FearWormCoilBarrageState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    isFinished_ = false;
    nextFireSeconds_ = 0.0f;
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Coil;
}

void FearWormCoilBarrageState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    battle.movementPatternTimer_ += TimeManager::GetInstance()->GetDeltaTime();
    battle.UpdateBattleMovement(boss, boss.player_->GetTranslate(), boss.MovementSpeedRate());
    FireBarrage(battle, boss);
    isFinished_ = battle.movementPatternTimer_ >= battle.kCoilFormationDurationSeconds
        + battle.kCoilAttackDurationSeconds + battle.kCoilReleaseDurationSeconds;
}

void FearWormCoilBarrageState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    nextFireSeconds_ = 0.0f;
}

void FearWormCoilBarrageState::FireBarrage(FearWormBattleState& battle, FearWormEnemy& boss)
{
    if (!battle.CanFire(boss) || !battle.IsCoilFiring()) {
        nextFireSeconds_ = 0.0f;
        return;
    }

    // 攻撃開始からの経過時間で、次の発射時刻を判定する。
    const float attackElapsedSeconds = battle.movementPatternTimer_ - battle.kCoilFormationDurationSeconds;
    if (attackElapsedSeconds < nextFireSeconds_) {
        return;
    }

    // 処理落ちからの復帰時も、同じフレームに大量の弾を生成しない。
    nextFireSeconds_ = attackElapsedSeconds + kCoilBarrageFireIntervalSeconds;

    // 360度を弾数で均等に分割
    float angleStep = (2.0f * battle.kPi) / static_cast<float>(kCoilBarrageBulletCount);
    // 全弾の速さを維持し、広がる速さと後方へ進む速さの比率を決める。
    const float backwardSpeedRatio = 1.0f / std::sqrt(1.0f + kCoilBarrageSpreadRatio * kCoilBarrageSpreadRatio);
    const float radialSpeedRatio = kCoilBarrageSpreadRatio * backwardSpeedRatio;

    // 生存している各部位から発射
    for (size_t segmentIndex = 0; segmentIndex < boss.segments_.size(); segmentIndex++) {

        if (!boss.segments_[segmentIndex].isAlive) {
            continue;
        }

        const Vector3 position = boss.segments_[segmentIndex].position;
        const float angleOffset = static_cast<float>(segmentIndex) * kCoilBarrageSegmentAngleRadians
            + battle.movementPatternTimer_ * kCoilBarrageAngularSpeedRadiansPerSecond;

        for (int32_t bulletIndex = 0; bulletIndex < kCoilBarrageBulletCount; bulletIndex++) {

            float angle = angleStep * static_cast<float>(bulletIndex) + angleOffset;

            // プレイヤー座標を使わず、XY平面で360度へ広げながら-Z方向へ進める。
            Vector3 direction = {
                std::cos(angle) * radialSpeedRatio,
                std::sin(angle) * radialSpeedRatio,
                -backwardSpeedRatio
            };

            battle.FireDirectionalBullet(boss, position, direction, false);
        }
        EffectManager::GetInstance()->PlayEffect("WormShotFlash", position);
    }
}
