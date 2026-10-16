// 発射間隔を空けながら、ボタンを押すたびにミサイルを発射する武器。
#pragma once
#include "App/Game/Player/Weapon/Common/BasePlayerWeapon.h"

class MissilePlayerWeapon : public BasePlayerWeapon {
public:
    const char* GetName() const override { return "Missile"; }
    // 長押しの継続では追加発射せず、押した瞬間だけ発射を要求する。
    void Update(PlayerWeaponContext& context, bool held, bool triggered, float dt) override;
    // 共通のミサイル待ち時間を確認し、発射できたら待ち時間を数え直す。
    bool Fire(PlayerWeaponContext& context) override;
    // ミサイルを作り、毎秒で設定された速さを弾の更新で使う単位へ変換する。
    std::unique_ptr<PlayerBullet> CreateBullet(Model* model, PlayerWeaponContext& context,
        BaseEnemy* target, float& speed) const override;
};
