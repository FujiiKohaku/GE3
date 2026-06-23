#pragma once

#include "../../Engine/Effect/EffectManager.h"
#include "PlayerBullet.h"

#include <memory>

class MissileBullet : public PlayerBullet {
public:
    void Initialize(Model* model) override;
    void Update() override;
    void SetDead() override;
   // void Move() override;
    float GetSpeed() const
    {
        return speed_;
    }
    void OnHitEnemy(const Vector3& position) override;

private:
    void CreateTrailEffect();
    void UpdateTrailPosition();
    void StopTrailEffect();

private:
    float speed_ = 40.0f;
    float scale_ = 2.0f;

    EffectHandle trailEffectHandle_ = kInvalidEffectHandle;

    std::shared_ptr<Vector3> trailPosition_;
};