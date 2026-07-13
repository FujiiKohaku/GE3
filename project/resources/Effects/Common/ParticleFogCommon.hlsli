#ifndef EFFECT_PARTICLE_FOG_COMMON_HLSLI
#define EFFECT_PARTICLE_FOG_COMMON_HLSLI

#include "ParticleCommon.hlsli"
#include "../../Shaders/Common/Fog.hlsli"

ConstantBuffer<FogData> gFog : register(b1);

float32_t4 ApplyParticleFog(float32_t4 color, float32_t3 worldPosition, float32_t viewDistance)
{
    return ApplyFog(color, gFog, worldPosition, viewDistance);
}

float32_t4 ApplyParticleFog(float32_t4 color, VertexShaderOutput input)
{
    return ApplyParticleFog(color, input.worldPosition, input.viewDistance);
}

#endif
