#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 六角形距離関数
float HexDist(float2 p)
{
    p = abs(p);
    float c = dot(p, normalize(float2(1.0f, 1.7320508f)));
    return max(c, p.x);
}

float4 HexGrid(float2 uv, float scale)
{
    uv *= scale;
    float2 r = float2(1.0f, 1.7320508f);
    float2 h = r * 0.5f;

    float2 a = uv - r * floor(uv / r);
    float2 b = (uv - h) - r * floor((uv - h) / r);

    float2 gv = (length(a - h) < length(b - h)) ? a - h : b - h;

    float d = HexDist(gv);
    float edge = smoothstep(0.42f, 0.46f, d) - smoothstep(0.46f, 0.50f, d);

    return float4(gv, d, edge);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;

    // 自機前方のスクリーンスクリーン位置を中心に設定
    float2 center = sonicBoomCenter;
    if (length(center) <= 0.0001f)
    {
        center = float2(0.5f, 0.5f);
    }

    float shieldPower = saturate(vignetteStrength);

    if (shieldPower <= 0.001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 16:9アスペクト比補正された中心からの距離
    float2 diff = (uv - center);
    diff.x *= 16.0f / 9.0f;

    float dist = length(diff);

    // バリアドームサイズ
    float barrierRadius = 0.32f;
    float barrierMask = smoothstep(barrierRadius + 0.05f, barrierRadius - 0.04f, dist);

    if (barrierMask <= 0.001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 時間経過によるバリアエネルギーの脈動パルス
    float pulse = sin(time * 6.0f - dist * 16.0f) * 0.5f + 0.5f;

    // 六角形ハニカムグリッドの生成
    float2 hexUV = (uv - center) * float2(16.0f / 9.0f, 1.0f);
    float4 hex = HexGrid(hexUV, 20.0f);

    float hexLine = hex.w;
    float hexFill = (1.0f - hex.z);

    // ★自機モデルを完全に最前面で遮蔽・発光する強烈な極太ネオンブルー
    float3 cyanShieldGlow = float3(0.30f, 1.20f, 2.00f);
    float3 blueShieldCore = float3(0.12f, 0.55f, 1.40f);

    float3 shieldColor = hexLine * cyanShieldGlow * 3.5f + hexFill * blueShieldCore * (0.50f + pulse * 0.50f);

    // バリア空間屈折
    float2 refractionOffset = diff * (hexLine * 0.015f + 0.006f) * barrierMask;
    float4 screenColor = gTexture.Sample(gSampler, uv + refractionOffset);

    // 自機モデルや背景の『手前（前面）』に強烈にオーバーレイ加算描画！
    screenColor.rgb = lerp(screenColor.rgb, shieldColor, barrierMask * 0.45f * shieldPower) + shieldColor * barrierMask * shieldPower;

    return screenColor;
}
