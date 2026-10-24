// 発射間隔を空けながら、ボタンを押すたびにミサイルを発射する武器。
#include "App/Game/Player/Combat/Weapon/Types/MissilePlayerWeapon.h"
#include "App/Game/Player/Combat/Bullet/Types/MissileBullet.h"
#include "Engine/audio/SoundManager.h"

// 長押しの継続では追加発射せず、押した瞬間だけ発射を要求する。
void MissilePlayerWeapon::Update(PlayerWeaponContext& context, bool, bool triggered, float)
{
    if (context.canFire && triggered) context.fire();
}

// 共通のミサイル待ち時間を確認し、発射できたら待ち時間を数え直す。
bool MissilePlayerWeapon::Fire(PlayerWeaponContext& context)
{
    if (!context.canFire || context.missileCooldown + 0.000001f < 2.0f) return false;
    context.missileCooldown = 0.0f;
    SoundManager::GetInstance()->Play("MissileShot");
    context.emit(nullptr);
    return true;
}

// ミサイルを作り、毎秒で設定された速さを弾の更新で使う単位へ変換する。
std::unique_ptr<PlayerBullet> MissilePlayerWeapon::CreateBullet(
    Model* model, PlayerWeaponContext&, BaseEnemy*, float& speed) const
{
    auto bullet = std::make_unique<MissileBullet>();
    speed = bullet->GetSpeed() / 60.0f;
    bullet->Initialize(model);
    return bullet;
}
