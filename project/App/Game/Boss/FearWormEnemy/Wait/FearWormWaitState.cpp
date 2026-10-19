#include "FearWormWaitState.h"
#include "../FearWormEnemy.h"
#include "App/Game/Player/Player.h"

void FearWormWaitState::Update(FearWormEnemy& boss)
{
    float movementSpeedRate = boss.MovementSpeedRate();
    Vector3 target;

    if (boss.player_ == nullptr) {
        target = boss.FallbackTarget();
    } else {
        Vector3 playerPosition = boss.player_->GetTranslate();

        // 起動条件チェック
        if (playerPosition.z >= boss.startPosition_.z - boss.activationLeadDistance_) {
            boss.ChangeState(FearWormEnemy::BossState::Entry); // 登場状態へ移行
            return;
        }

        target = boss.EntryStartPosition(playerPosition);
    }

    // 移動処理
    boss.segments_[0].position = Lerp(boss.segments_[0].position, target, boss.moveSpeed_ * movementSpeedRate);
    boss.transform_.translate = boss.segments_[0].position;
}
