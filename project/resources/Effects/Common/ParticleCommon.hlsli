#ifndef EFFECT_PARTICLE_COMMON_HLSLI
#define EFFECT_PARTICLE_COMMON_HLSLI

#include "../../Shaders/Common/Particle.hlsli"

struct ParticleRenderParameter
{
    float32_t dissolveThreshold;
    float32_t emissionStrength;
    float32_t uvScrollSpeedX;
    float32_t uvScrollSpeedY;
};

#endif
