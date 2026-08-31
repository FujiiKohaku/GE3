#include "../../Shaders/Common/Particle.hlsli"

ConstantBuffer<EmitterSphere> gEmitter : register(b0);
RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<EffectSettings> gEffectSettings : register(b2);

static const int32_t kMaxGPUParticle = 4096;
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
        (float)DTid.x * 37.71f + gPerFrame.time * 9.3f,
        (float)DTid.x * 67.19f - gPerFrame.time * 4.1f,
        (float)DTid.x * 99.83f + gPerFrame.time * 6.7f
    );
    float32_t3 rnd = frac(sin(rndSeed) * 43758.5453f);

    float theta = rnd.x * kPi * 2.0f;
    float phi = acos(2.0f * rnd.y - 1.0f);

    float speed = lerp(8.0f, 19.0f, rnd.z);
    float32_t3 sphereVel = float32_t3(
        sin(phi) * cos(theta) * speed,
        cos(phi) * speed + 1.2f,
        sin(phi) * sin(theta) * speed
    );

    float scale = lerp(gEffectSettings.startScale * 0.7f, gEffectSettings.startScale * 1.6f, rnd.x);
    float lifeTime = lerp(gEffectSettings.lifeTime * 0.8f, gEffectSettings.lifeTime * 1.2f, rnd.y);

    gParticles[particleIndex].translate = gEmitter.translate + (rnd - 0.5f) * 0.4f;
    gParticles[particleIndex].velocity = sphereVel;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;

    // 鮮烈なオレンジ〜金朱色カラーグラデーション
    float32_t4 orangeColor = lerp(
        float32_t4(1.0f, 0.65f, 0.1f, 1.0f),  // 眩しいオレンジ黄金
        float32_t4(0.95f, 0.18f, 0.0f, 1.0f), // 朱赤
        rnd.x
    );
    gParticles[particleIndex].color = orangeColor;
    gParticles[particleIndex].rotation = rnd.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (rnd.x - 0.5f) * gEffectSettings.rotationSpeed;
}
