#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"
#include "Explosion.hlsli"

PixelShaderOutput main(ExplosionVertexOutput input)
{
    const float3 kOuterColor = float3(0.70f, 0.035f, 0.015f);
    const float3 kFlameColor = float3(1.0f, 0.25f, 0.025f);
    const float3 kCoreColor = float3(1.0f, 0.84f, 0.13f);
    const float3 kFlashColor = float3(1.0f, 0.97f, 0.72f);
    float lifeRatio = input.lifeRatio;
    float2 position = input.texcoord * 2.0f - 1.0f;
    float radius = length(position);
    float angle = atan2(position.y, position.x);
    float lobes = sin(angle * 7.0f + lifeRatio * 2.5f) * 0.075f;
    lobes += sin(angle * 11.0f - lifeRatio * 3.0f) * 0.035f;
    float boundary = 0.77f + lobes;
    float signedDistance = radius - boundary;
    float feather = max(fwidth(signedDistance), 0.008f);
    float coverage = 1.0f - smoothstep(-feather, feather, signedDistance);

    // 赤い縁、オレンジの炎、黄色い中心を段階的に重ねる。
    float flameMask = 1.0f - smoothstep(-0.11f - feather, -0.11f + feather, signedDistance);
    float coreBoundary = lerp(0.52f, 0.12f, smoothstep(0.12f, 0.70f, lifeRatio));
    float coreDistance = radius - coreBoundary - lobes * 0.65f;
    float coreMask = 1.0f - smoothstep(-feather, feather, coreDistance);
    float3 flameColor = lerp(kOuterColor, kFlameColor, flameMask);
    flameColor = lerp(flameColor, kCoreColor, coreMask);

    // 後半は中央から抜け、炎の塊が分裂して消える。
    float breakup = smoothstep(0.42f, 0.88f, lifeRatio);
    float holeRadius = breakup * 0.82f;
    float holeDistance = radius - holeRadius - lobes * 0.90f;
    float hollowMask = smoothstep(-feather, feather, holeDistance);
    float fragmentMask = smoothstep(breakup * 0.85f - 0.16f,
        breakup * 0.85f + 0.04f, sin(angle * 7.0f + lifeRatio * 2.5f) * 0.5f + 0.5f);
    coverage *= hollowMask * fragmentMask;

    // 最初の一瞬だけ、鋭い小さな閃光を見せる。
    float flash = 1.0f - smoothstep(0.02f, 0.15f, lifeRatio);
    float flashMask = AnimeStarburstMask(position);
    coverage = lerp(coverage, flashMask, flash);
    flameColor = lerp(flameColor, kFlashColor, flash);
    float4 color = float4(flameColor * gMaterial.color.rgb,
        coverage * input.color.a * gMaterial.color.a);
    if (color.a <= gMaterial.alphaReference) {
        discard;
    }
    color = ApplyParticleFog(color, input.worldPosition, input.viewDistance);
    return MakeParticlePixelOutput(color);
}
