#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 数学的ノイズ関数（ノイズテクスチャ不要で高精細ディゾルブノイズを生成）
float hash21(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float noise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = hash21(i);
    float b = hash21(i + float2(1.0f, 0.0f));
    float c = hash21(i + float2(0.0f, 1.0f));
    float d = hash21(i + float2(1.0f, 1.0f));
    float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(a, b, u.x) + (c - a) * u.y * (1.0f - u.x) + (d - b) * u.x * u.y;
}

float fbm(float2 p)
{
    float val = 0.0f;
    float amp = 0.5f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        val += amp * noise(p);
        p *= 2.05f;
        amp *= 0.5f;
    }
    return val;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 mainColor = gTexture.Sample(gSampler, input.texcoord);

    // プロシージャル・ディゾルブ・ノイズマップ
    float noiseValue = fbm(input.texcoord * 11.0f);

    // 進行閾値 (vignetteStrength をディゾルブ進行度 0.0〜1.0 として共有利用)
    float threshold = saturate(vignetteStrength);

    if (threshold <= 0.001f)
    {
        return mainColor;
    }

    // 完全消滅領域 (焦げた暗闇)
    if (noiseValue < threshold)
    {
        return float4(0.02f, 0.01f, 0.01f, 1.0f);
    }

    // 燃え広がる赤・オレンジの焼き切りエネルギー線エッジ
    float edgeWidth = 0.12f;
    float edgeDist = noiseValue - threshold;

    if (edgeDist < edgeWidth)
    {
        float edgeRatio = 1.0f - (edgeDist / edgeWidth);
        // 赤く熱した炎の輝きエッジ
        float3 fireEdgeColor = lerp(float3(1.0f, 0.35f, 0.05f), float3(1.2f, 0.9f, 0.2f), edgeRatio);
        mainColor.rgb = lerp(mainColor.rgb, fireEdgeColor, edgeRatio * 0.95f);
    }

    return mainColor;
}
