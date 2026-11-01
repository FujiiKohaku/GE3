#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;

    // vignetteStrength を衝撃波の進行度 (0.0:発生 〜 1.0:拡散消滅) として使用
    float progress = saturate(vignetteStrength);

    if (progress <= 0.001f || progress >= 0.999f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 画面中心からのディストーション (16:9アスペクト比補正)
    float2 center = float2(0.5f, 0.5f);
    float2 diff = uv - center;
    diff.x *= 16.0f / 9.0f;

    float dist = length(diff);

    // 進行度に応じて画面中心から外側へ広がる超太い衝撃波リングの半径
    float waveRadius = progress * 1.10f;
    float ringWidth = 0.18f; // くっきり極太リング

    // リング線上の強度計算
    float ringDist = abs(dist - waveRadius);
    float waveFactor = smoothstep(ringWidth, 0.0f, ringDist);

    // 進行に伴って消滅する減衰
    waveFactor *= smoothstep(1.0f, 0.0f, progress);

    if (waveFactor <= 0.001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 中心から放射状に広がる大迫力歪み屈折 (強度を 0.12f へ超絶アップ！)
    float2 offsetDir = normalize(diff);
    float distortion = sin((dist - waveRadius) * 35.0f) * waveFactor * 0.12f;
    float2 distortedUV = input.texcoord + offsetDir * distortion;

    float4 color = gTexture.Sample(gSampler, distortedUV);

    // 衝撃波リングのエッジを鮮やかに光らせる
    float3 shockwaveLight = float3(0.90f, 1.20f, 1.50f);
    color.rgb += waveFactor * 0.65f * shockwaveLight;

    return color;
}
