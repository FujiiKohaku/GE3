#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gWaterPattern : register(t1);
SamplerState gSampler : register(s0);

float SampleWaterHeight(float2 uv)
{
    // 低いYタイリングで模様を縦長にし、速度の違う二層を下方向へ流す。
    float2 broadUV = frac(float2(uv.x * 1.35f, uv.y * 0.28f - time * 0.10f));
    float2 detailUV = frac(float2(uv.x * 2.70f + 0.37f, uv.y * 0.52f - time * 0.17f));
    float broad = gWaterPattern.SampleLevel(gSampler, broadUV, 0.0f).r;
    float detail = gWaterPattern.SampleLevel(gSampler, detailUV, 0.0f).r;
    return broad * 0.72f + detail * 0.28f;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float intensity = saturate(waterEffectIntensity);

    const float sampleStep = 0.0035f;
    float centerHeight = SampleWaterHeight(uv);
    float heightX = SampleWaterHeight(uv + float2(sampleStep, 0.0f));
    float heightY = SampleWaterHeight(uv + float2(0.0f, sampleStep));
    float2 waterNormal = float2(centerHeight - heightX, centerHeight - heightY) / sampleStep;

    // 流れる法線で元画面のUVをずらして水膜の屈折を作る。
    float streamMask = smoothstep(0.28f, 0.62f, centerHeight);
    float2 refractionOffset = waterNormal * 0.00175f * streamMask * intensity;
    float2 distortedUV = saturate(uv + refractionOffset);
    float4 color = gTexture.Sample(gSampler, distortedUV);

    // 水膜の明るい筋を薄く重ねる。円形の輪郭は描かない。
    float highlight = smoothstep(0.60f, 0.82f, centerHeight) * intensity;
    color.rgb += float3(0.035f, 0.085f, 0.12f) * highlight;

    return color;
}
