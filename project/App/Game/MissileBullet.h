#pragma once
#include "Bullet.h"
#include "../../Engine/Effect/EffectManager.h"
#include <memory>

class MissileBullet : public Bullet {
public:
    void Initialize(Model* model) override;
    void Update() override;
    void SetDead() override;
    void OnFired() override;

    float GetSpeed() const
    {
        return speed_;
    }

    const char* GetHitEffectName() const override
    {
        return "MissileExplosion";
    }

    bool IsDestroyedOnHit() const override
    {
        return true;
    }

private:
    void UpdateTrailPosition();
    void StopTrailEffect();

    float speed_ = 40.0f;
    float scale_ = 2.0f;
    EffectHandle trailEffectHandle_ = kInvalidEffectHandle;
    std::shared_ptr<Vector3> trailPosition_;
};
