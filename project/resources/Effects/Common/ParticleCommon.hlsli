#ifndef EFFECT_PARTICLE_COMMON_HLSLI
#define EFFECT_PARTICLE_COMMON_HLSLI

#include "../../Shaders/Common/Particle.hlsli"

struct ParticleRenderParameter
{
    float32_t dissolveThreshold;
    float32_t emissionStrength;
    float32_t uvScrollSpeedX;
    float32_t uvScrollSpeedY;
    float32_t4 trailStartColor;
    float32_t4 trailEndColor;
    float32_t trailLifeTime;
    float32_t trailStartWidth;
    float32_t trailEndWidth;
    float32_t trailTextureTiling;
    float32_t trailMinVertexDistance;
    float32_t trailBreakDistance;
    uint32_t maxTrailPoints;
    int32_t faceCamera;
    float32_t trailRootExtension;
    float32_t3 trailPadding;
};

struct TrailPoint
{
    float32_t3 position;
    float32_t age;
    uint32_t isActive;
    float32_t3 padding;
};

#endif
