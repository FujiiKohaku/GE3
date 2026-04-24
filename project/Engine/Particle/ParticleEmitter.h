#pragma once
#include "ParticleData.h"
#include <random>

class ParticleEmitter {
public:
    void Initialize();

    Particle MakeParticleDefault(const Vector3& position);
    Particle MakeFireParticle(const Vector3& position);
    Particle MakeNewParticleAttack(const Vector3& position);
    Particle MakeRingParticle(const Vector3& position);

private:
    std::random_device seedGenerator_;
    std::mt19937 randomEngine_;
};