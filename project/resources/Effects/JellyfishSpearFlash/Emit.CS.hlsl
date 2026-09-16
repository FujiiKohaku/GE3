#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
static const float kPi = 3.14159265f;

float Hash1d(float seed)
{
    return frac(sin(seed * 78.233f) * 43758.5453f);
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

    const uint32_t particleIndex = gFreeList[freeListIndex];
    const float seed = float(id.x) + gPerFrame.time * 17.0f;
    const float randomA = Hash1d(seed + 0.31f);
    const float randomB = Hash1d(seed + 1.79f);
    const float angle = float(id.x) / max(float(gEmitter.count), 1.0f) * kPi * 2.0f;
    const bool isCenterFlash = id.x == 0;

    float3 velocity = float3(cos(angle), sin(angle), (randomB - 0.5f) * 0.28f) *
        (9.0f + randomA * 6.0f);
    float scale = gEffectSettings.startScale * (0.28f + randomB * 0.24f);
    float lifeTime = 0.09f + randomA * 0.055f;
    if (isCenterFlash) {
        velocity = float3(0.0f, 0.0f, 0.0f);
        scale = gEffectSettings.startScale * 1.65f;
        lifeTime = 0.075f;
    }

    gParticles[particleIndex].translate = gEmitter.translate;
    gParticles[particleIndex].velocity = velocity;
    gParticles[particleIndex].scale = float3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    gParticles[particleIndex].rotation = angle + randomA * 0.3f;
    gParticles[particleIndex].rotationSpeed = (randomB - 0.5f) * 18.0f;
}
