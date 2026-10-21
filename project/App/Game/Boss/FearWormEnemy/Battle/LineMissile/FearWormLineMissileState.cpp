#include "FearWormLineMissileState.h"
#include "../FearWormBattleState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"
#include "Engine/Time/TimeManager.h"
#include "Engine/Effect/EffectManager.h"
#include "Engine/3D/Object3d.h"
#include "App/Game/Enemy/Bullet/Types/TrackingEnemyBullet.h"
#include <memory>
#include <utility>
#include <cmath>
#include <algorithm>

void FearWormLineMissileState::Enter(FearWormBattleState& battle, FearWormEnemy&)
{
    isFinished_ = false;
    phase_ = Phase::Formation;
    phaseTimerSeconds_ = 0.0f;
    shotCount_ = 0;
    targetIndices_.clear();
    battle.movementPattern_ = FearWormBattleState::MovementPattern::Line;
    battle.lineTransitionTimer_ = 0.0f;
    battle.missileAttackTimer_ = 0.0f;
}

void FearWormLineMissileState::Update(FearWormBattleState& battle, FearWormEnemy& boss)
{
    const float deltaTimeSeconds = TimeManager::GetInstance()->GetDeltaTime();
    battle.movementPatternTimer_ += deltaTimeSeconds * boss.MovementSpeedRate();
    battle.UpdateBattleMovement(boss, boss.player_->GetTranslate(), boss.MovementSpeedRate());
    phaseTimerSeconds_ += deltaTimeSeconds;
    switch (phase_) {
    case Phase::Formation:
        if (phaseTimerSeconds_ >= kMissileAimWaitDurationSeconds) {
            for (size_t index = 0; index < boss.segments_.size(); index++) {
                if (boss.segments_[index].isAlive) {
                    targetIndices_.push_back(index);
                }
            }
            phase_ = Phase::Fire;
            phaseTimerSeconds_ = kMissileFireIntervalSeconds;
        }
        break;
    case Phase::Fire:
        if (shotCount_ >= static_cast<int32_t>(targetIndices_.size())) {
            phase_ = Phase::Recovery;
            phaseTimerSeconds_ = 0.0f;
            break;
        }
        if (phaseTimerSeconds_ >= kMissileFireIntervalSeconds) {
            phaseTimerSeconds_ = 0.0f;
            const size_t segmentIndex = targetIndices_[shotCount_];
            if (battle.CanFire(boss) && segmentIndex < boss.segments_.size()
                && boss.segments_[segmentIndex].isAlive) {
                FireSingleMissile(battle, boss, segmentIndex);
            }
            shotCount_++;
        }
        break;
    case Phase::Recovery:
        isFinished_ = phaseTimerSeconds_ >= kMissilePostFireCooldownSeconds;
        break;
    }
}

void FearWormLineMissileState::Exit(FearWormBattleState&, FearWormEnemy&)
{
    targetIndices_.clear();
    shotCount_ = 0;
    phaseTimerSeconds_ = 0.0f;
}

void FearWormLineMissileState::FireSingleMissile(FearWormBattleState& battle, FearWormEnemy& boss, size_t segmentIndex)
{
    if (boss.player_ == nullptr || battle.GetBulletModel() == nullptr) {
        return;
    }

    std::unique_ptr<TrackingEnemyBullet> bullet = std::make_unique<TrackingEnemyBullet>();
    bullet->Initialize(battle.GetBulletModel());
    bullet->SetPlayer(boss.player_);

    // 発射座標を設定
    bullet->SetTranslate(boss.segments_[segmentIndex].position);

    // 発射エフェクトの再生
    EffectManager::GetInstance()->PlayEffect("WormShotFlash", boss.segments_[segmentIndex].position);

    // ボスの弾リストに追加
    boss.AddEnemyBullet(std::move(bullet));
}
