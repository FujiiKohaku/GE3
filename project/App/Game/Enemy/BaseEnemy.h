#pragma once

#include "Engine/3D/Object3d.h"
#include <memory>
#include <vector>

#include "Engine/math/EngineStruct.h"

#include "App/Game/Enemy/Bullet/EnemyBullet.h"
class Camera;
class Model;

struct EnemyCollisionPart {
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    float radius = 3.0f;
    int32_t partIndex = 0;
};

class BaseEnemy {
public:
    virtual ~BaseEnemy() = default;

    virtual void Initialize(Model* model);
    virtual void Update();
    virtual void Draw();

    virtual void Move();
    virtual void Attack();

    bool IsDead() const;

    virtual Vector3 GetPosition() const;
    std::vector<std::unique_ptr<EnemyBullet>>& GetBullets()
    {
        return enemyBullets_;
    }
    virtual void SetPosition(const Vector3& position);
    void SetEnableLighting(bool enable);
    void SetDead(bool isDead);
    void ApplyDamage(float damage);
    virtual void ApplyDamageToPart(int32_t partIndex, float damage);
    virtual void GetCollisionParts(std::vector<EnemyCollisionPart>& parts) const;
    virtual bool IsCollisionPartDamageable(int32_t partIndex) const;
    virtual void OnCollisionPartGuarded(int32_t partIndex, const Vector3& position);

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
