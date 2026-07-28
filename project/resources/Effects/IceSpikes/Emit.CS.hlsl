#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 1024;
static const float kPi = 3.14159265f;

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

    float32_t3 rndSeed = float32_t3(
        (float)DTid.x * 21.41f + gPerFrame.time * 6.3f,
        (float)DTid.x * 49.19f - gPerFrame.time * 3.7f,
        (float)DTid.x * 73.83f + gPerFrame.time * 5.1f
    );
    float32_t3 rnd = frac(sin(rndSeed) * 43758.5453f);

    float angle = rnd.x * kPi * 2.0f;
    float dist = lerp(0.5f, gEmitter.radius, rnd.y);

    // 地面から斜め上空へ突っ切る立体氷柱初速
    float32_t3 spikeVel = float32_t3(
        cos(angle) * lerp(2.0f, 6.0f, rnd.z),
        lerp(6.0f, 13.0f, rnd.y),
        sin(angle) * lerp(2.0f, 6.0f, rnd.z)
    );

    float scale = lerp(gEffectSettings.startScale * 0.7f, gEffectSettings.startScale * 1.8f, rnd.x);

    gParticles[particleIndex].translate = float32_t3(
        gEmitter.translate.x + cos(angle) * dist,
        -4.9f,
        gEmitter.translate.z + sin(angle) * dist
    );
    gParticles[particleIndex].velocity = spikeVel;
    gParticles[particleIndex].scale = float32_t3(scale, scale * 2.5f, scale); // 縦長に伸びる立体氷柱
    gParticles[particleIndex].lifeTime = gEffectSettings.lifeTime * lerp(0.8f, 1.2f, rnd.z);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = rnd.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (rnd.x - 0.5f) * gEffectSettings.rotationSpeed;
}
