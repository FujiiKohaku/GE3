#pragma once
#include "PlayerWeaponHeat.h"
#include "PlayerHomingLock.h"
#include "App/Game/Player/Bullet/PlayerBullet.h"
#include <functional>

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
    void Update(PlayerWeaponContext& context, bool held, bool triggered, float dt) override;
    bool Fire(PlayerWeaponContext& context) override;
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

class NormalPlayerWeapon final : public AutomaticPlayerWeapon {
public:
    NormalPlayerWeapon() : AutomaticPlayerWeapon(10.0f / 60.0f, 3, false, "NormalShot") {}
    const char* GetName() const override { return "Normal"; }
};

class MinigunPlayerWeapon final : public AutomaticPlayerWeapon {
public:
    MinigunPlayerWeapon() : AutomaticPlayerWeapon(3.0f / 60.0f, 1, true, "MinigunShot") {}
    const char* GetName() const override { return "Minigun"; }
};

class MissilePlayerWeapon : public BasePlayerWeapon {
public:
    const char* GetName() const override { return "Missile"; }
    void Update(PlayerWeaponContext& context, bool held, bool triggered, float dt) override;
    bool Fire(PlayerWeaponContext& context) override;
    std::unique_ptr<PlayerBullet> CreateBullet(Model* model, PlayerWeaponContext& context,
        BaseEnemy* target, float& speed) const override;
};

class HomingPlayerWeapon final : public MissilePlayerWeapon {
public:
    const char* GetName() const override { return "Homing Missile"; }
    bool IsHoming() const override { return true; }
    void Update(PlayerWeaponContext& context, bool held, bool triggered, float dt) override;
    bool Fire(PlayerWeaponContext& context) override;
    std::unique_ptr<PlayerBullet> CreateBullet(Model* model, PlayerWeaponContext& context,
        BaseEnemy* target, float& speed) const override;
};
