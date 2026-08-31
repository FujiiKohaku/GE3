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
        (float)DTid.x * 17.31f + gPerFrame.time * 9.7f,
        (float)DTid.x * 47.89f - gPerFrame.time * 5.3f,
        (float)DTid.x * 79.13f + gPerFrame.time * 6.1f
    );
    float32_t3 rnd = frac(sin(rndSeed) * 43758.5453f);

    float scale = lerp(gEffectSettings.startScale * 0.5f, gEffectSettings.startScale * 1.6f, rnd.x);
    float lifeTime = lerp(gEffectSettings.lifeTime * 0.7f, gEffectSettings.lifeTime * 1.3f, rnd.y);

    // 床の全方位・放射状・花火爆発初速ベクトル
    float theta = rnd.x * kPi * 2.0f;
    float phi = (rnd.y * 0.4f + 0.1f) * kPi; // 上向き半球状の噴出

    float speed = lerp(6.0f, 15.0f, rnd.z);
    float32_t3 burstVel = float32_t3(
        sin(phi) * cos(theta) * speed,
        cos(phi) * speed + 3.0f,
        sin(phi) * sin(theta) * speed
    );

    float32_t3 spawnOffset = float32_t3(
        sin(phi) * cos(theta) * gEmitter.radius * rnd.x,
        -4.9f,
        sin(phi) * sin(theta) * gEmitter.radius * rnd.x
    );

    gParticles[particleIndex].translate = float32_t3(gEmitter.translate.x, 0.0f, gEmitter.translate.z) + spawnOffset;
    gParticles[particleIndex].velocity = burstVel;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;

    // 青・シアン・サファイア・白熱のランダムカラー花火
    float32_t4 sparkColor = lerp(
        float32_t4(0.2f, 0.95f, 1.0f, 1.0f),
        float32_t4(0.85f, 0.98f, 1.0f, 1.0f),
        rnd.x
    );
    gParticles[particleIndex].color = sparkColor;
    gParticles[particleIndex].rotation = rnd.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (rnd.x - 0.5f) * gEffectSettings.rotationSpeed;
}
