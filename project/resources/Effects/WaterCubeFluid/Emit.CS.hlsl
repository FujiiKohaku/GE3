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
    if (gEmitter.emit == 0)
    {
        return;
    }

    if (DTid.x >= gEmitter.count)
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

    // 乱数用シードの計算
    float32_t3 rndSeed = float32_t3(
        (float)DTid.x * 17.13f + gPerFrame.time * 2.1f,
        (float)DTid.x * 43.51f - gPerFrame.time * 1.7f,
        (float)DTid.x * 91.27f + gPerFrame.time * 3.3f
    );
    float32_t3 rnd = frac(sin(rndSeed) * 43758.5453f);

    // 立方体（Cube）領域内ランダム生成
    float32_t3 boxOffset = (rnd - 0.5f) * 2.0f * gEmitter.radius;

    // 初速（流体ノイズ方向の微小速度）
    float32_t3 initialVel = (rnd - 0.5f) * 0.8f + gEffectSettings.velocity;

    float scale = lerp(gEffectSettings.startScale * 0.8f, gEffectSettings.startScale * 1.2f, rnd.x);
    float lifeTime = lerp(gEffectSettings.lifeTime * 0.7f, gEffectSettings.lifeTime * 1.3f, rnd.y);

    gParticles[particleIndex].translate = gEmitter.translate + boxOffset;
    gParticles[particleIndex].velocity = initialVel;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = rnd.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (rnd.x - 0.5f) * gEffectSettings.rotationSpeed;
}
