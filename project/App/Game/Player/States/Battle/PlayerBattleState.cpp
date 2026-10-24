#include "PlayerBattleState.h"
#include "App/Game/Player/Player.h"
#include "Engine/Input/Input.h"
#include "Engine/Time/TimeManager.h"

void PlayerBattleState::Update(Player& player)
{
    Input* input = Input::GetInstance();
    if (input == nullptr) {
        return;
    }
    player.UpdateInvincibility(TimeManager::GetInstance()->GetDeltaTime());
    player.UpdateBoostState();
    if (!player.isDebugMode_) {
        // 照準を先に確定してから操舵し、発射と表示で参照する位置を揃える。
        // 使用中の入力機器から照準を取得し、操舵とレティクル表示に使う。
        player.UpdateAimFromInput(*input, TimeManager::GetInstance()->GetDeltaTime());
        player.ClampAimScreenPosition();
        player.ConstrainGamepadAimToPlayer();
        player.UpdateMovement(input);
    }
    player.ApplyRailPosition();
    if (!player.isDebugMode_) {
        player.ConstrainGamepadAimToPlayer();
        if (player.camera_ != nullptr) {
            // 移動後の機体との距離を補正した照準で、発射に使う奥行きを確定する。
            player.UpdateSmoothedAimDistance(*player.camera_, TimeManager::GetInstance()->GetDeltaTime());
        }
    }
    player.ApplyTransform();
    // 最新の照準と移動後の銃口から発射し、弾はこの後で1回だけ更新する。
    player.weaponController_.UpdateInput(input, player.isDebugMode_);
    player.weaponController_.UpdateBullets();
    // 寿命切れや命中で無効になった弾を一覧から取り除く。
    player.weaponController_.RemoveDeadBullets();
    player.object_->Update();
#ifdef _DEBUG
    player.weaponController_.DrawAimDebugLines();
#endif
}
