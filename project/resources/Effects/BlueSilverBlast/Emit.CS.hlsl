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
        (float)DTid.x * 23.11f + gPerFrame.time * 9.1f,
        (float)DTid.x * 41.79f - gPerFrame.time * 4.3f,
        (float)DTid.x * 83.23f + gPerFrame.time * 7.7f
    );
    float32_t3 rnd = frac(sin(rndSeed) * 43758.5453f);

    // 【青銀乱】360度全方向（真球状）への美しい均一破裂ベクトル
    float theta = rnd.x * kPi * 2.0f;
    float phi = acos(2.0f * rnd.y - 1.0f); // 真球状の全方向均一分布

    float speed = lerp(10.0f, 22.0f, rnd.z);
    float32_t3 sphereVel = float32_t3(
        sin(phi) * cos(theta) * speed,
        cos(phi) * speed + 1.5f,
        sin(phi) * sin(theta) * speed
    );

    float scale = lerp(gEffectSettings.startScale * 0.6f, gEffectSettings.startScale * 1.5f, rnd.x);
    float lifeTime = lerp(gEffectSettings.lifeTime * 0.8f, gEffectSettings.lifeTime * 1.2f, rnd.y);

    gParticles[particleIndex].translate = gEmitter.translate + (rnd - 0.5f) * 0.4f;
    gParticles[particleIndex].velocity = sphereVel;
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);
    gParticles[particleIndex].lifeTime = lifeTime;
    gParticles[particleIndex].currentTime = 0.0f;

    // 画像通りの白銀コア ＋ サファイア・青紫グラデーションカラー
    float32_t4 sparkColor = lerp(
        float32_t4(0.85f, 0.95f, 1.0f, 1.0f),  // 白銀
        float32_t4(0.35f, 0.25f, 0.95f, 1.0f), // 青紫
        rnd.x
    );
    gParticles[particleIndex].color = sparkColor;
    gParticles[particleIndex].rotation = rnd.z * kPi * 2.0f;
    gParticles[particleIndex].rotationSpeed = (rnd.x - 0.5f) * gEffectSettings.rotationSpeed;
}
