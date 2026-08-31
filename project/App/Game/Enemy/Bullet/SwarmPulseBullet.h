#pragma once

#include "App/Game/Enemy/Bullet/EnemyBullet.h"

class SwarmPulseBullet : public EnemyBullet {
public:
    void Initialize(Model* model) override;
    void SetTranslate(const Vector3& translate) override;
    void SetSwarmVelocity(const Vector3& velocity);
    void SetWavePhase(float phase);

protected:
    void Move() override;

private:
    Vector3 pathPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 waveSide_ = { 1.0f, 0.0f, 0.0f };
    Vector3 waveUp_ = { 0.0f, 1.0f, 0.0f };
    float waveTime_ = 0.0f;
    float wavePhase_ = 0.0f;
    float waveAmplitude_ = 0.85f;
    float waveFrequency_ = 8.0f;
};
