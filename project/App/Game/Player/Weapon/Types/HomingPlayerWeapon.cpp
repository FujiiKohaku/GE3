// 射撃を長押しして敵をロックし、離すとロック対象へミサイルを発射する武器。
#include "App/Game/Player/Weapon/Types/HomingPlayerWeapon.h"
#include "App/Game/Player/Bullet/Types/HomingMissileBullet.h"
#include "Engine/audio/SoundManager.h"

// 押している間はロックを更新し、離した際に発射できたらロックを解除する。
void HomingPlayerWeapon::Update(PlayerWeaponContext& context, bool held, bool, float)
{
    if (context.lock.UpdateFireInput(held, context.canFire, context.camera,
        context.position, context.forward, context.aim)) {
        if (context.fire()) context.lock.ClearTargets();
    }
}

// ミサイルの待ち時間を確認し、ロック対象ごとに発射する。対象がなければ通常方向へ一発撃つ。
bool HomingPlayerWeapon::Fire(PlayerWeaponContext& context)
{
    if (!context.canFire || context.missileCooldown + 0.000001f < 2.0f) return false;
    context.missileCooldown = 0.0f;
    SoundManager::GetInstance()->Play("MissileShot");
    if (context.lock.GetLockedTargets().empty()) {
        context.emit(nullptr);
    } else {
        for (BaseEnemy* target : context.lock.GetLockedTargets()) context.emit(target);
    }
    return true;
}

// 追尾ミサイルを生成し、対象と生存確認用の敵一覧を設定する。
std::unique_ptr<PlayerBullet> HomingPlayerWeapon::CreateBullet(
    Model* model, PlayerWeaponContext& context, BaseEnemy* target, float& speed) const
{
    auto bullet = std::make_unique<HomingMissileBullet>();
    speed = bullet->GetSpeed() / 60.0f;
    bullet->Initialize(model);
    bullet->SetTarget(target, context.lock.GetTargets());
    return bullet;
}
