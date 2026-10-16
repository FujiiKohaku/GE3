// 通常弾とミニガンに共通する連射間隔・発熱・弾生成の処理をまとめる。
#pragma once
#include "App/Game/Player/Weapon/Common/BasePlayerWeapon.h"

// 通常弾とミニガンに共通する連射動作。発熱量・威力・間隔は派生側が指定する。
class AutomaticPlayerWeapon : public BasePlayerWeapon {
public:
    bool UsesHeat() const override { return true; }
    // 初押しで即座に撃ち、長押し中は経過時間に応じた発数を要求する。
    void Update(PlayerWeaponContext& context, bool held, bool triggered, float dt) override;
    // 射撃可能かと過熱状態を確認し、弾の生成・発射音・発熱を一回分処理する。
    bool Fire(PlayerWeaponContext& context) override;
    // 通常弾を生成し、武器の基本威力に現在の熱量による倍率を反映する。
    std::unique_ptr<PlayerBullet> CreateBullet(Model* model, PlayerWeaponContext& context,
        BaseEnemy* target, float& speed) const override;
protected:
    AutomaticPlayerWeapon(float interval, int damage, bool minigun, const char* sound)
        : interval_(interval), damage_(damage), minigun_(minigun), sound_(sound) {}
private:
    float interval_;
    int damage_;
    bool minigun_;
    const char* sound_;
    float cooldown_ = 0.0f;
};
