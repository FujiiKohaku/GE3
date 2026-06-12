#pragma once

#include "../../Engine/3D/Object3d.h"
#include <memory>

#include "../../Engine/math/EngineStruct.h"

#include "EnemyBullet.h"
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

    virtual void OnHit();

    bool IsDead() const;

    Vector3 GetPosition() const;
    void SetPosition(const Vector3& position);
    void SetDead(bool isDead) { isDead_ = isDead; }
    std::vector<std::unique_ptr<EnemyBullet>>& GetBullets()
    {
        return enemyBullets_;
    }

protected:
    std::unique_ptr<Object3d> object_;

    EulerTransform transform_;

    bool isDead_ = false;

    int hp_ = 1;

    float moveSpeed_ = 0.1f;

    std::vector<std::unique_ptr<EnemyBullet>> enemyBullets_;
};