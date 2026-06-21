#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
static const float kPi = 3.14159265f;

float Hash(float value)
{
    return frac(sin(value) * 43758.5453f);
}

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || DTid.x >= gEmitter.count)
    {
        return;
    }

    int32_t freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    if (freeListIndex < 0 || freeListIndex >= kMaxGPUParticle)
    {
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        return;
    }

    uint32_t particleIndex = gFreeList[freeListIndex];

    float randomA = Hash((float) DTid.x * 11.7f + gPerFrame.time * 17.9f);
    float randomB = Hash((float) DTid.x * 19.5f + 3.4f);
    float randomC = Hash((float) DTid.x * 29.1f + 8.2f);
    float angle = randomA * kPi * 2.0f;
    float radius = randomB * gEmitter.radius * 1.4f;
    float segmentZ = randomC * 46.0f;
    float scale = gEffectSettings.startScale * (0.7f + Hash((float) DTid.x * 7.4f) * 0.8f);

    gParticles[particleIndex].translate =
        gEmitter.translate +
        float32_t3(cos(angle) * radius, sin(angle) * radius, segmentZ);
    gParticles[particleIndex].velocity =
        gEffectSettings.velocity +
        float32_t3(cos(angle), sin(angle), 0.0f) * (2.0f + randomB * 6.0f);
    gParticles[particleIndex].scale =
        float32_t3(scale, scale * 2.5f, scale);
    gParticles[particleIndex].lifeTime =
        gEffectSettings.lifeTime * (0.75f + Hash((float) DTid.x * 5.3f) * 0.5f);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = angle;
    gParticles[particleIndex].rotationSpeed = 0.0f;
}
