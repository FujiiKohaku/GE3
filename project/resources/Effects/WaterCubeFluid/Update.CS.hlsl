#include "../../Shaders/Common/Particle.hlsli"

RWStructuredBuffer<ParticleCS> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);
ConstantBuffer<PerFrame> gPerFrame : register(b0);
ConstantBuffer<EffectSettings> gEffectSettings : register(b1);
ConstantBuffer<EmitterSphere> gEmitter : register(b2);

static const uint32_t kMaxGPUParticle = 1024;

// カールノイズ風の渦ベクトル計算
float32_t3 CalculateFluidCurl(float32_t3 pos, float32_t time)
{
    float32_t pX = pos.x * 0.8f + time * 1.2f;
    float32_t pY = pos.y * 0.8f + time * 1.5f;
    float32_t pZ = pos.z * 0.8f - time * 1.1f;

    float32_t3 v1 = float32_t3(sin(pY + pZ), sin(pZ + pX), sin(pX + pY));
    float32_t3 v2 = float32_t3(cos(pZ * 1.3f), cos(pX * 1.3f), cos(pY * 1.3f));
    return cross(v1, v2);
}

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;

    if (particleIndex >= kMaxGPUParticle || gParticles[particleIndex].color.a == 0.0f)
    {
        return;
    }

    float32_t3 currentPos = gParticles[particleIndex].translate;

    // 1. 流体ノイズ（Curl Noise風の渦運動）の計算
    if (gEffectSettings.enableNoise != 0)
    {
        float32_t3 curlVel = CalculateFluidCurl(currentPos - gEmitter.translate, gPerFrame.time);
        gParticles[particleIndex].velocity += curlVel * gEffectSettings.noiseStrength * gPerFrame.deltaTime;
    }

    // 2. 表面張力（立方体領域内への引き戻し・吸引力）
    if (gEffectSettings.enableAttraction != 0)
    {
        float32_t3 toCenter = gEmitter.translate - currentPos;
        float32_t distToCenter = length(toCenter);
        float32_t boxRadius = gEmitter.radius;

        // 立方体境界の外側、あるいは中心から離れるほど引き戻し力を強化
        if (distToCenter > boxRadius * 0.5f)
        {
            float32_t pullFactor = (distToCenter / max(boxRadius, 0.001f));
            gParticles[particleIndex].velocity += normalize(toCenter) * (pullFactor * gEffectSettings.attractionStrength) * gPerFrame.deltaTime;
        }
    }

    // 3. 空気抵抗 / 粘性ドラッグ
    if (gEffectSettings.enableDrag != 0)
    {
        gParticles[particleIndex].velocity *= pow(max(gEffectSettings.drag, 0.0f), gPerFrame.deltaTime * 60.0f);
    }

    // 座標・寿命・回転更新
    gParticles[particleIndex].translate += gParticles[particleIndex].velocity * gPerFrame.deltaTime;
    gParticles[particleIndex].currentTime += gPerFrame.deltaTime;
    gParticles[particleIndex].rotation += gParticles[particleIndex].rotationSpeed * gPerFrame.deltaTime;

    float lifeRate = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);
    float scale = lerp(gEffectSettings.startScale, gEffectSettings.endScale, lifeRate);
    gParticles[particleIndex].scale = float32_t3(scale, scale, scale);

    // 水色からディープブルー・エメラルドへの透明度グラデーション
    gParticles[particleIndex].color = lerp(gEffectSettings.startColor, gEffectSettings.endColor, lifeRate);

    // 寿命完了時のフリーリスト戻し
    if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime ||
        gParticles[particleIndex].color.a <= 0.0f)
    {
        gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);

        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);

        if ((freeListIndex + 1) < kMaxGPUParticle)
        {
            gFreeList[freeListIndex + 1] = particleIndex;
        }
        else
        {
            InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        }
    }
}
