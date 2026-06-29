#pragma once

#include "Engine/3D/Object3d.h"
#include <memory>
#include <vector>

#include "Engine/math/EngineStruct.h"

#include "App/Game/Enemy/Bullet/EnemyBullet.h"
class Camera;
class Model;

class BaseEnemy {
public:
    virtual ~BaseEnemy() = default;

    virtual void Initialize(Model* model);
    virtual void Update();
    virtual void Draw();

    virtual void Move();
    virtual void Attack();

    bool IsDead() const;

    Vector3 GetPosition() const;
    std::vector<std::unique_ptr<EnemyBullet>>& GetBullets()
    {
        return enemyBullets_;
    }
    void SetPosition(const Vector3& position);
    void SetDead(bool isDead);
    void ApplyDamage(float damage);

protected:
    virtual void UpdateAnimation();
    virtual void OnDamage(float damage);
    virtual void OnDeath();

    std::unique_ptr<Object3d> object_;

    EulerTransform transform_;

    bool isDead_ = false;

    float hp_ = 1.0f;

    float moveSpeed_ = 0.1f;

    std::vector<std::unique_ptr<EnemyBullet>> enemyBullets_;
};
