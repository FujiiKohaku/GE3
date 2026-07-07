#pragma once
#include "BaseBoss.h"
#include <memory>

class Object3d;

class FearBoss : public BaseBoss {
public:
    FearBoss();
    ~FearBoss() override = default;

    void Initialize(Model* model) override;
    void Update(const Vector3& playerPosition) override;
    void Draw() override;

    bool IsDead() const override { return hp_ <= 0; }
    void ApplyDamage(float damage) override;

    std::vector<BossCollider>& GetColliders() override { return colliders_; }

private:
    std::unique_ptr<Object3d> object_;
    int hp_ = 10;
    std::vector<BossCollider> colliders_;
    float timer_ = 0.0f;
};
