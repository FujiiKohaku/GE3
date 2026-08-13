#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
static const float kPi = 3.14159265f;

float Random(float value)
{
    return frac(sin(value * 12.9898f) * 43758.5453f);
}

[numthreads(256, 1, 1)]
void main(uint32_t3 id : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || id.x >= gEmitter.count) return;

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= kMaxGPUParticle) {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];
    float seed = float(id.x) * 31.73f + gPerFrame.time * 97.11f;
    float angle = Random(seed) * kPi * 2.0f;
    float radius = sqrt(Random(seed + 4.7f)) * gEmitter.radius;
    float3 radial = float3(sin(angle), 0.0f, cos(angle));
    float scale = lerp(gEffectSettings.startScale * 0.55f, gEffectSettings.startScale * 1.15f, Random(seed + 9.2f));

    gParticles[particleIndex].translate = gEmitter.translate + radial * radius + float3(0.0f, 0.08f, 0.0f);
    gParticles[particleIndex].velocity = radial * lerp(0.08f, 0.42f, Random(seed + 2.3f)) +
        float3(0.0f, lerp(0.65f, 1.75f, Random(seed + 7.6f)), 0.0f);
    gParticles[particleIndex].scale = float3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lerp(0.72f, gEffectSettings.lifeTime, Random(seed + 11.8f));
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = 0.0f;
    gParticles[particleIndex].rotationSpeed = 0.0f;
}
