#include "App/Game/Common/CombatTarget.h"
// アサルトの連射間隔・発熱・弾生成の処理をまとめる。
#include "App/Game/Player/Combat/Weapon/Common/BasePlayerWeapon.h"
#include "App/Game/Player/Combat/Bullet/Types/NormalBullet.h"
#include "App/Game/Player/Combat/Weapon/PlayerWeaponController.h"
#include "Engine/audio/SoundManager.h"
#include <algorithm>
#include <cmath>

void CooldownPlayerWeapon::UpdateCooldown(float deltaTimeSeconds)
{
    if (!std::isfinite(deltaTimeSeconds) || deltaTimeSeconds <= 0.0f) { return; }
    elapsedCooldownSeconds_ = (std::min)(static_cast<double>(fireIntervalSeconds_),
        elapsedCooldownSeconds_ + static_cast<double>(deltaTimeSeconds));
}

float CooldownPlayerWeapon::GetReloadRatio() const
{
    if (IsReadyToFire()) { return 1.0f; }
    return std::clamp(static_cast<float>(elapsedCooldownSeconds_ / fireIntervalSeconds_), 0.0f, 1.0f);
}

bool CooldownPlayerWeapon::IsReadyToFire() const
{
    constexpr float kCooldownToleranceSeconds = 0.000001f;
    return elapsedCooldownSeconds_ + kCooldownToleranceSeconds >= fireIntervalSeconds_;
}

bool CooldownPlayerWeapon::TryStartCooldown()
{
    if (!IsReadyToFire()) { return false; }
    elapsedCooldownSeconds_ = 0.0f;
    return true;
}

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
    context.heat.AddShotHeat();
    return true;
}

// 通常弾を生成し、武器の基本威力に現在の熱量による倍率を反映する。
std::unique_ptr<PlayerBullet> AutomaticPlayerWeapon::CreateBullet(
    Model* model, PlayerWeaponContext& context, CombatTarget*, float&) const
{
    auto bullet = std::make_unique<NormalBullet>();
    bullet->Initialize(model);
    bullet->SetDamage(context.heat.GetShotDamage(damage_));
    return bullet;
}
