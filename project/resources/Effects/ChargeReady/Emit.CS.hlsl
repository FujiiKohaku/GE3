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

    float angle = Hash((float) DTid.x * 13.9f + gPerFrame.time * 17.0f) * kPi * 2.0f;
    float height = Hash((float) DTid.x * 5.2f) * 2.0f - 1.0f;
    float radius = gEmitter.radius + Hash((float) DTid.x * 9.1f) * 1.4f;
    float32_t3 direction = normalize(float32_t3(cos(angle), height * 0.45f, sin(angle)));
    float speed = 8.0f + Hash((float) DTid.x * 3.4f) * 18.0f;
    float scale = gEffectSettings.startScale * (0.7f + Hash((float) DTid.x * 8.6f) * 0.9f);

    gParticles[particleIndex].translate = gEmitter.translate + direction * radius;
    gParticles[particleIndex].velocity = direction * speed;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = gEffectSettings.lifeTime * (0.75f + Hash((float) DTid.x * 4.6f) * 0.6f);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = angle;
    gParticles[particleIndex].rotationSpeed = gEffectSettings.rotationSpeed * (0.5f + Hash((float) DTid.x * 2.2f));
}
