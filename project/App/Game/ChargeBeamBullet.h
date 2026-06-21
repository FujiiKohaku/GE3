#pragma once
#include "Bullet.h"

class ChargeBeamBullet : public Bullet {
public:
    void Initialize(Model* model) override;
    void Update() override;
    void Draw() override;

    void Configure(int chargeLevel, const Vector3& position);

    bool IsChargeBeam() const override
    {
        return true;
    }

    Vector3 GetAABBMin() const override
    {
        return aabbMin_;
    }

    Vector3 GetAABBMax() const override
    {
        return aabbMax_;
    }

    const char* GetHitEffectName() const override
    {
        return "BeamHit";
    }

    int GetChargeLevel() const
    {
        return chargeLevel_;
    }

    float GetLength() const
    {
        return length_;
    }

    float GetHalfWidth() const
    {
        return halfWidth_;
    }

private:
    void ApplyLevelParameters();
    void UpdateAABB();

    int chargeLevel_ = 1;
    float length_ = 50.0f;
    float halfWidth_ = 1.2f;
    Vector3 aabbMin_ = { 0.0f, 0.0f, 0.0f };
    Vector3 aabbMax_ = { 0.0f, 0.0f, 0.0f };
};
