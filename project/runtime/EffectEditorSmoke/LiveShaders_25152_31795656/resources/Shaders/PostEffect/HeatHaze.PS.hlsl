#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 数学的ノイズ関数（熱気カゲロウの揺らめき用）
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

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;

    // vignetteStrength をミニガンの熱気蓄積量 (0.0 〜 1.0) として利用
    float heat = saturate(vignetteStrength);

    if (heat <= 0.001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 画面中央（レティクル・照準・弾道エリア）は 100% 完全クリア（揺らぎ・ブレを完全にシャットアウト！）
    float2 centerDist = abs(uv - float2(0.5f, 0.5f));
    float centerClearMask = smoothstep(0.18f, 0.45f, length(centerDist * float2(1.25f, 1.0f)));

    // 画面最下部（銃身の上部エリア: uv.y >= 0.72f）
    float bottomMask = smoothstep(0.68f, 0.95f, uv.y);

    // 熱気カゲロウを画面最下部および画面外周（画面端）のみに厳格限定！
    float heatAreaMask = max(bottomMask, centerClearMask * 0.65f) * heat;

    if (heatAreaMask <= 0.001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 上方向へゆらゆら流れるカゲロウ波形
    float2 noiseUV = uv * float2(16.0f, 9.0f) + float2(0.0f, -uv.y * 3.2f);
    float wave1 = noise(noiseUV) - 0.5f;
    float wave2 = noise(noiseUV * 1.8f + float2(2.1f, 1.4f)) - 0.5f;
    float combinedWave = (wave1 + wave2 * 0.5f);

    // エイム領域に干渉しない、画面下部・画面周縁部限定の熱気屈折ディストーション
    float2 distortedUV = uv + float2(combinedWave * 0.016f, combinedWave * 0.008f) * heatAreaMask;

    float4 color = gTexture.Sample(gSampler, distortedUV);

    // 画面最下部の銃身周辺にほんのり赤オレンジの熱気光
    float3 heatGlow = float3(0.14f, 0.03f, 0.0f);
    color.rgb += heatGlow * heatAreaMask;

    return color;
}
