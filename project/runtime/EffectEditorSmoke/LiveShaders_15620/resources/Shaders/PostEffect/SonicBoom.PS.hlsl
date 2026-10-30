#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// Ease-In Cubic イージング関数
float easeInCubic(float x)
{
    return x * x * x;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;

    // 専用進行度パラメータを参照
    float linearProgress = saturate(sonicBoomProgress);

    if (linearProgress <= 0.001f || linearProgress >= 0.999f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // Ease-Inイージング適用: 最初溜まるようにゆっくり ➔ 後半爆発的に加速して画面外へ！
    float progress = easeInCubic(linearProgress);

    // 自機のスクリーン位置を中心発生源に設定！(デフォルトは画面中央)
    float2 center = sonicBoomCenter;
    if (length(center) <= 0.0001f)
    {
        center = float2(0.5f, 0.5f);
    }

    float2 diff = uv - center;
    diff.x *= 16.0f / 9.0f; // 16:9アスペクト比補正

    float dist = length(diff);

    // 自機を中心とした進行度に応じた爆散拡散半径
    float waveRadius = progress * 1.50f;
    float ringWidth = 0.12f + linearProgress * 0.08f;

    float ringDist = abs(dist - waveRadius);
    float waveFactor = smoothstep(ringWidth, 0.0f, ringDist);

    // 後半にかけて光芒と歪みが一気に最高潮に達するイージング減衰
    float fadeFactor = sin(linearProgress * 3.14159265f);
    waveFactor *= fadeFactor;

    if (waveFactor <= 0.001f)
    {
        return gTexture.Sample(gSampler, uv);
    }

    // 自機から外側へ広がる音速到達時の空間屈折（加速する歪み屈折）
    float2 offsetDir = (dist > 0.0001f) ? normalize(diff) : float2(0.0f, 0.0f);
    float distortion = sin((dist - waveRadius) * 30.0f) * waveFactor * 0.075f;
    float2 distortedUV = input.texcoord + offsetDir * distortion;

    float4 color = gTexture.Sample(gSampler, distortedUV);

    // 自機から放出される青・シアンの音速突破グラデーション光芒エッジ (Sonic Boom Glow)
    float3 cyanSonicGlow = float3(0.25f, 0.90f, 1.45f);
    color.rgb += waveFactor * 1.20f * cyanSonicGlow;

    return color;
}
