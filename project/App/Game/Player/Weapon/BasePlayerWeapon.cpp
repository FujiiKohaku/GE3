#include "BasePlayerWeapon.h"
#include "App/Game/Player/Bullet/NormalBullet.h"
#include "App/Game/Player/Bullet/MissileBullet.h"
#include "App/Game/Player/Bullet/HomingMissileBullet.h"
#include "Engine/audio/SoundManager.h"
#include <algorithm>
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

bool AutomaticPlayerWeapon::Fire(PlayerWeaponContext& context)
{
    if (!context.canFire || context.heat.IsOverheated()) return false;
    SoundManager::GetInstance()->Play(sound_);
    context.emit(nullptr);
    context.heat.AddShotHeat(minigun_);
    return true;
}

std::unique_ptr<PlayerBullet> AutomaticPlayerWeapon::CreateBullet(
    Model* model, PlayerWeaponContext& context, BaseEnemy*, float&) const
{
    auto bullet = std::make_unique<NormalBullet>();
    bullet->Initialize(model);
    bullet->SetDamage(context.heat.GetShotDamage(damage_));
    return bullet;
}

void MissilePlayerWeapon::Update(PlayerWeaponContext& context, bool, bool triggered, float)
{
    if (context.canFire && triggered) context.fire();
}

bool MissilePlayerWeapon::Fire(PlayerWeaponContext& context)
{
    if (!context.canFire || context.missileCooldown + 0.000001f < 2.0f) return false;
    context.missileCooldown = 0.0f;
    SoundManager::GetInstance()->Play("MissileShot");
    context.emit(nullptr);
    return true;
}

std::unique_ptr<PlayerBullet> MissilePlayerWeapon::CreateBullet(
    Model* model, PlayerWeaponContext&, BaseEnemy*, float& speed) const
{
    auto bullet = std::make_unique<MissileBullet>();
    speed = bullet->GetSpeed() / 60.0f;
    bullet->Initialize(model);
    return bullet;
}

void HomingPlayerWeapon::Update(PlayerWeaponContext& context, bool held, bool, float)
{
    if (context.lock.UpdateFireInput(held, context.canFire, context.camera,
        context.position, context.forward, context.aim)) {
        if (context.fire()) context.lock.ClearTargets();
    }
}

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

std::unique_ptr<PlayerBullet> HomingPlayerWeapon::CreateBullet(
    Model* model, PlayerWeaponContext& context, BaseEnemy* target, float& speed) const
{
    auto bullet = std::make_unique<HomingMissileBullet>();
    speed = bullet->GetSpeed() / 60.0f;
    bullet->Initialize(model);
    bullet->SetTarget(target, context.lock.GetTargets());
    return bullet;
}

