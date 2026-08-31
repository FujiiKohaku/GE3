#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// プロシージャルハッシュノイズ
float hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031f);
    p3 += dot(p3, p3.yzx + 33.33f);
    return frac((p3.x + p3.y) * p3.z);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;

    // 時間経過による走査線の流動速度
    float t = time * 2.0f;

    // 微小なRGB色収差（サイバーホログラムの色ズレ）
    float aberration = 0.0025f;
    float r = gTexture.Sample(gSampler, uv + float2(-aberration, 0.0f)).r;
    float g = gTexture.Sample(gSampler, uv).g;
    float b = gTexture.Sample(gSampler, uv + float2(aberration, 0.0f)).b;

    float4 color = float4(r, g, b, 1.0f);

    // 高精細な水平走査線（スキャンライン）
    float scanline = sin(uv.y * 600.0f) * 0.5f + 0.5f;
    scanline = pow(scanline, 1.5f);

    // 上から下へ流れるSF戦術ホログラム主走査バー (Sweeping Scan Bar)
    float sweepBar = smoothstep(0.06f, 0.0f, abs(frac(uv.y * 0.5f - t * 0.15f) - 0.5f));

    // サイバー微細デジタルノイズ
    float noise = hash12(uv * 500.0f + float2(t, t)) * 0.04f;

    // スキャンラインと掃引バーの明るさ調整
    float lineIntensity = lerp(0.82f, 1.0f, scanline) + sweepBar * 0.18f + noise;

    color.rgb *= lineIntensity;

    // ほんのりネオンブルー・シアンのSFホログラム光沢を加算
    float3 cyanHologramGlow = float3(0.04f, 0.18f, 0.28f);
    color.rgb += cyanHologramGlow * (scanline * 0.35f + sweepBar * 0.6f);

    return color;
}
