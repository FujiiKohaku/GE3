// 通常弾とミニガンに共通する連射間隔・発熱・弾生成の処理をまとめる。
#include "App/Game/Player/Combat/Weapon/Common/BasePlayerWeapon.h"
#include "App/Game/Player/Combat/Bullet/Types/NormalBullet.h"
#include "App/Game/Player/Combat/Weapon/PlayerWeaponController.h"
#include "Engine/audio/SoundManager.h"
#include <algorithm>

// 初押しで即座に撃ち、長押し中は経過時間に応じた発数を要求する。
void AutomaticPlayerWeapon::Update(PlayerWeaponContext& context, bool held, bool triggered, float dt)
{
    if (!context.canFire) return;
    if (triggered) {
        cooldown_ = 0.0f;
        context.fire();
    }
    if (held) {
        cooldown_ += dt;
        while (cooldown_ + 0.000001f >= interval_) {
            cooldown_ = (std::max)(0.0f, cooldown_ - interval_);
            context.fire();
        }
    }
}

// 射撃可能かと過熱状態を確認し、弾の生成・発射音・発熱を一回分処理する。
bool AutomaticPlayerWeapon::Fire(PlayerWeaponContext& context)
{
    if (!context.canFire || context.heat.IsOverheated()) return false;
    SoundManager::GetInstance()->Play(sound_);
    context.emit(nullptr);
    context.heat.AddShotHeat(minigun_);
    return true;
}

// 通常弾を生成し、武器の基本威力に現在の熱量による倍率を反映する。
std::unique_ptr<PlayerBullet> AutomaticPlayerWeapon::CreateBullet(
    Model* model, PlayerWeaponContext& context, BaseEnemy*, float&) const
{
    auto bullet = std::make_unique<NormalBullet>();
    bullet->Initialize(model);
    bullet->SetDamage(context.heat.GetShotDamage(damage_));
    return bullet;
}
