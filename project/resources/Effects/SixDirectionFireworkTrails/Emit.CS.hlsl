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
        (float)DTid.x * 29.31f + gPerFrame.time * 7.7f,
        (float)DTid.x * 57.19f - gPerFrame.time * 3.1f,
        (float)DTid.x * 89.83f + gPerFrame.time * 6.3f
    );
    float32_t3 rnd = frac(sin(rndSeed) * 43758.5453f);

    // 【6方向飛翔】氷の中心から 60° 刻みの6方向（0°, 60°, 120°, 180°, 240°, 300°）へ射出
    uint32_t dirIndex = DTid.x % 6;
    float dirAngle = (float)dirIndex * (kPi / 3.0f) + (rnd.x - 0.5f) * 0.08f; // わずかなばらつき

    float pitchAngle = lerp(0.2f, 0.45f, rnd.y); // 上空方向への角度
    float speed = lerp(20.0f, 32.0f, rnd.z);

    float32_t3 launchVel = float32_t3(
        cos(dirAngle) * cos(pitchAngle) * speed,
        sin(pitchAngle) * speed + 3.0f,
        sin(dirAngle) * cos(pitchAngle) * speed
    );

    float scale = lerp(gEffectSettings.startScale * 0.8f, gEffectSettings.startScale * 1.5f, rnd.x);

    gParticles[particleIndex].translate = float32_t3(gEmitter.translate.x, -4.8f, gEmitter.translate.z);
    gParticles[particleIndex].velocity = launchVel;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = gEffectSettings.lifeTime * lerp(0.85f, 1.15f, rnd.y);
    gParticles[particleIndex].currentTime = 0.0f;
    gParticles[particleIndex].color = gEffectSettings.startColor;
    gParticles[particleIndex].rotation = rnd.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (rnd.x - 0.5f) * gEffectSettings.rotationSpeed;
}
