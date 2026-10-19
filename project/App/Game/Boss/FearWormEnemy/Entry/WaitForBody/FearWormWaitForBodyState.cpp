#include "FearWormWaitForBodyState.h"
#include "../FearWormEntryState.h"
#include "../../FearWormEnemy.h"
#include "App/Game/Player/Player.h"

void FearWormWaitForBodyState::Update(FearWormEntryState& entry, FearWormEnemy& boss)
{
    // 頭の到着後も、同じ登場時刻で後方の部位を戦闘位置へ進める。
    if (!boss.segments_.empty()) {
        const Vector3 playerPosition = boss.player_->GetTranslate();
        const Vector3 surfacePosition = entry.EntrySurfacePosition(playerPosition);
        const Vector3 battlePosition = boss.MovementTargetPosition(playerPosition);
        for (size_t index = 0; index < boss.segments_.size(); ++index) {
            boss.segments_[index].position = FearWormEntryState::CalculateEntryPosition(
                surfacePosition, battlePosition, entry.elapsedSeconds_, index, boss.segmentSpacing_);
        }
        boss.transform_.translate = boss.segments_[0].position;
    }
    // 大きな時間飛びで飛び出し段階を越えた場合も、水しぶきを一度だけ出す。
    entry.jumOutState_.PlaySplashIfNeeded(entry, boss);
    entry.jumOutState_.UpdateSurface(entry, boss);
    if (entry.elapsedSeconds_ >= entry.durationSeconds_) {
        boss.ChangeState(FearWormEnemy::BossState::Battle);
    }
}
