// 武器が必要とする共有情報と、更新・発射・弾生成の共通インターフェースを定義する。
#pragma once
#include "App/Game/Player/Combat/Bullet/Common/PlayerBullet.h"
#include <functional>

class PlayerWeaponHeat;
class PlayerHomingLock;
class BaseEnemy;

// 武器が参照する機体状態と共有状態。管理クラス側の値を参照し、武器ごとの重複保持を避ける。
struct PlayerWeaponContext {
    PlayerWeaponHeat& heat;
    PlayerHomingLock& lock;
    float& missileCooldown;
    Camera* camera;
    Vector3 position;
    Vector3 forward;
    Vector2 aim;
    bool canFire;
    std::function<bool()> fire;
    std::function<void(BaseEnemy*)> emit;
};

// 武器に共通する更新・発射・弾生成を、選択中の基底ポインタから呼び出す。
class BasePlayerWeapon {
public:
    virtual ~BasePlayerWeapon() = default;
    virtual const char* GetName() const = 0;
    virtual bool IsHoming() const { return false; }
    virtual bool UsesHeat() const { return false; }
    virtual void Update(PlayerWeaponContext& context, bool held, bool triggered, float dt) = 0;
    virtual bool Fire(PlayerWeaponContext& context) = 0;
    virtual std::unique_ptr<PlayerBullet> CreateBullet(Model* model, PlayerWeaponContext& context,
        BaseEnemy* target, float& speed) const = 0;
};

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
