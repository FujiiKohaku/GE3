// 武器が必要とする共有情報と、更新・発射・弾生成の共通インターフェースを定義する。
#pragma once
#include "App/Game/Player/Weapon/Common/PlayerWeaponHeat.h"
#include "App/Game/Player/Weapon/Common/PlayerHomingLock.h"
#include "App/Game/Player/Bullet/Common/PlayerBullet.h"
#include <functional>

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
