#include "ParticleSystem.h"

void ParticleSystem::Initialize()
{
    emitter_.Initialize();

     ParticleManager::GetInstance()->CreateParticleGroup("Default","resources/circle.png",ParticleMeshManager::ParticleMeshType::Board);
}

void ParticleSystem::Emit(ParticleEffectType type, const Vector3& position, uint32_t count)
{
    for (uint32_t particleIndex = 0; particleIndex < count; particleIndex++) {
        if (type == ParticleEffectType::Ring) {
            EmitRing(position);
        }

        if (type == ParticleEffectType::Attack) {
            EmitAttack(position);
        }

        if (type == ParticleEffectType::Fire) {
            EmitFire(position);
        }
        if (type== ParticleEffectType::Normal)
        {
            EmitNormal(position);
        }
    }
}

void ParticleSystem::EmitRing(const Vector3& position)
{
    Particle particle = emitter_.MakeRingParticle(position);
    ParticleManager::GetInstance()->AddParticle("Ring", particle);
}

void ParticleSystem::EmitAttack(const Vector3& position)
{
    Particle particle = emitter_.MakeNewParticleAttack(position);
    ParticleManager::GetInstance()->AddParticle("Default", particle);
}

void ParticleSystem::EmitFire(const Vector3& position)
{
    Particle particle = emitter_.MakeFireParticle(position);
    ParticleManager::GetInstance()->AddParticle("Default", particle);
}
void ParticleSystem::EmitNormal(const Vector3& position)
{
    Particle particle = emitter_.MakeParticleDefault(position);
    ParticleManager::GetInstance()->AddParticle("Default", particle);
}