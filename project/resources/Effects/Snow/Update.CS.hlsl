#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex >= gEmitter.maxParticles || gParticles[particleIndex].color.a == 0.0f)
    {
        return;
    }

    // 風のひらひらゆらぎノイズ
    if (gEffectSettings.enableNoise != 0)
    {
        float32_t3 noiseVel = MakeNoise(particleIndex, gPerFrame.time * 0.5f);
        gParticles[particleIndex].velocity += float32_t3(noiseVel.x * 0.8f, noiseVel.y * 0.15f, noiseVel.z * 0.8f) * gEffectSettings.noiseStrength * gPerFrame.deltaTime;
    }

    if (gEffectSettings.enableDrag != 0)
    {
        gParticles[particleIndex].velocity *= pow(max(gEffectSettings.drag, 0.0f), gPerFrame.deltaTime * 60.0f);
    }

    // 落下移動
    gParticles[particleIndex].translate += gParticles[particleIndex].velocity * gPerFrame.deltaTime;
    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;
    gParticles[particleIndex].rotation += gParticles[particleIndex].rotationSpeed * gPerFrame.deltaTime;

    // 基本生存割合に基づくカラー補間
    float lifeRate = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
    float4 baseColor = lerp(gEffectSettings.startColor, gEffectSettings.endColor, lifeRate);

    // 【重要】地面付近（Y = -3.2f 〜 -4.9f）でのアルファ値減算フェードアウト
    float groundFade = saturate((gParticles[particleIndex].translate.y - (-4.9f)) / 1.7f);
    baseColor.a *= groundFade;

    gParticles[particleIndex].color = baseColor;

    // 地面（Y <= -4.9f）に達したか寿命・アルファ値完了で回収
    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].translate.y <= -4.9f ||
        gParticles[particleIndex].color.a <= 0.001f)
    {
        gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);
        gParticles[particleIndex].color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);

        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

        if ((freeListIndex + 1) < gEmitter.maxParticles)
        {
            gFreeList[freeListIndex + 1] = particleIndex;
        }
        else
        {
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        }
    }
}
