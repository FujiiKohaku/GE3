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
        (float)DTid.x * 43.71f + gPerFrame.time * 6.9f,
        (float)DTid.x * 73.19f - gPerFrame.time * 5.1f,
        (float)DTid.x * 103.83f + gPerFrame.time * 4.7f
    );
    float32_t3 rnd = frac(sin(rndSeed) * 43758.5453f);

    float angle = rnd.x * kPi * 2.0f;
    float dist = sqrt(rnd.y) * 4.5f;

    float32_t3 spawnPos = float32_t3(
        gEmitter.translate.x + cos(angle) * dist,
        gEmitter.translate.y + (rnd.z - 0.5f) * 0.5f,
        gEmitter.translate.z + sin(angle) * dist
    );

    float speed = lerp(1.5f, 4.0f, rnd.z);
    float32_t3 initVel = float32_t3(
        -sin(angle) * speed * 2.0f,
        speed * 1.5f,
        cos(angle) * speed * 2.0f
    );

    float scale = lerp(gEffectSettings.startScale * 0.7f, gEffectSettings.startScale * 1.5f, rnd.x);

    gParticles[particleIndex].translate = spawnPos;
    gParticles[particleIndex].velocity = initVel;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = gEffectSettings.lifeTime * lerp(0.8f, 1.2f, rnd.y);
    gParticles[particleIndex].currentTime = 0.0f;

    float32_t4 tornadoColor = lerp(
        float32_t4(1.0f, 0.75f, 0.15f, 0.95f),
        float32_t4(0.2f, 0.85f, 1.0f, 0.95f),
        rnd.x
    );
    gParticles[particleIndex].color = tornadoColor;
    gParticles[particleIndex].rotation = rnd.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (rnd.x - 0.5f) * gEffectSettings.rotationSpeed;
}
