#pragma once
#include "../math/MathStruct.h"
#include"../math/Object3DStruct.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include <cstdint>

enum class ParticleEffectType {
    Ring,
    Attack,
    Fire,
    Normal
};

class ParticleSystem {
public:
    void Initialize();

    void Emit(ParticleEffectType type, const Vector3& position, uint32_t count);

private:
    void EmitRing(const Vector3& position);
    void EmitAttack(const Vector3& position);
    void EmitFire(const Vector3& position);
    void EmitNormal(const Vector3& position);

private:
    ParticleEmitter emitter_;
};