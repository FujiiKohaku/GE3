// 射撃を長押しして敵をロックし、離すとロック対象へミサイルを発射する武器。
#pragma once
#include "App/Game/Player/Combat/Weapon/Types/MissilePlayerWeapon.h"

class HomingPlayerWeapon final : public MissilePlayerWeapon {
public:
    const char* GetName() const override { return "Homing Missile"; }
    bool IsHoming() const override { return true; }
    // 押している間はロックを更新し、離した際に発射できたらロックを解除する。
    void Update(PlayerWeaponContext& context, bool held, bool triggered, float dt) override;
    // ミサイルの待ち時間を確認し、ロック対象ごとに発射する。対象がなければ通常方向へ一発撃つ。
    bool Fire(PlayerWeaponContext& context) override;
    // 追尾ミサイルを生成し、対象と生存確認用の敵一覧を設定する。
    std::unique_ptr<PlayerBullet> CreateBullet(Model* model, PlayerWeaponContext& context,
        BaseEnemy* target, float& speed) const override;
};
