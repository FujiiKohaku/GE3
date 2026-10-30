#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// ランダムハッシュ関数
float2 hash22(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031f, 0.1030f, 0.0973f));
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.xx + p3.yz) * p3.zy);
}

float hash21(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

// 不規則で鋭いランダムビキビキ亀裂（Voronoi Edge Distance with Random Jitter）
float organicGlassCrack(float2 uv)
{
    // 不規則なグリッド配置
    float2 gridScale = uv * float2(11.0f, 7.5f);
    float2 i_st = floor(gridScale);
    float2 f_st = frac(gridScale);

    float minDistance1 = 8.0f;
    float minDistance2 = 8.0f;
    float2 closestPoint = float2(0, 0);

    // 不均一なノード探索
    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float2 neighbor = float2(float(x), float(y));
            // 点のランダム位置（完全ランダムなジッターで規則性を破壊）
            float2 pointPos = neighbor + hash22(i_st + neighbor);

            float dist = length(pointPos - f_st);
            if (dist < minDistance1)
            {
                minDistance2 = minDistance1;
                minDistance1 = dist;
                closestPoint = pointPos;
            }
            else if (dist < minDistance2)
            {
                minDistance2 = dist;
            }
        }
    }

    // 鋭い境界線（ビキビキとした亀裂線）
    float edgeDist = minDistance2 - minDistance1;
    return edgeDist;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;

    // 画面中央は完全にクリア、画面端ほどビキビキ亀裂が不規則に浸食
    float2 centerDist = abs(uv - float2(0.5f, 0.5f));
    float edgeMask = smoothstep(0.25f, 0.78f, length(centerDist * float2(1.25f, 1.0f)));

    if (edgeMask <= 0.001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 不規則なビキビキ亀裂マップ
    float crackPattern1 = organicGlassCrack(uv);
    float crackPattern2 = organicGlassCrack(uv * 1.8f + float2(3.5f, 7.2f));

    // 鋭い亀裂ライン (ビキビキッとした細く鋭い不規則折れ線)
    float line1 = smoothstep(0.022f, 0.001f, crackPattern1);
    float line2 = smoothstep(0.015f, 0.001f, crackPattern2);

    float totalCrack = saturate((line1 + line2 * 0.75f) * edgeMask);

    // 屈折（ビキビキ亀裂部分での背景のリアルな像のズレ）
    float2 refractOffset = float2(
        (hash21(uv * 28.0f) - 0.5f) * 0.018f,
        (hash21(uv * 28.0f + 3.1f) - 0.5f) * 0.018f
    ) * totalCrack;

    float4 color = gTexture.Sample(gSampler, uv + refractOffset);

    // クッキリと白く輝く鋭いガラス割れ目エッジ
    float3 glassWhite = float3(0.96f, 0.98f, 1.0f);
    color.rgb = lerp(color.rgb, glassWhite, totalCrack * 0.88f);

    // 割れ目のわずかな陰影
    color.rgb *= (1.0f - totalCrack * 0.25f);

    return color;
}
