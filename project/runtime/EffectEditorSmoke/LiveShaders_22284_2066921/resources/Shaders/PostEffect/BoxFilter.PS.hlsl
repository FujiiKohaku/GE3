#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width;
    uint height;
    gTexture.GetDimensions(width, height);

    // ビネット強度/フェードパラメータ (1.0 -> 0.0 で徐々にサンプリング距離が縮小)
    float blurStrength = saturate(vignetteStrength);

    // 時間経過とともにサンプリングステップが 3.8px から 0.0px へスムーズにフェードアウト
    float2 uvStepSize = float2((3.8f * blurStrength) / float(width), (3.8f * blurStrength) / float(height));

    float3 resultColor = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;

    // 5x5 ボックスフィルタフェード
    [unroll]
    for (int x = -2; x <= 2; ++x)
    {
        [unroll]
        for (int y = -2; y <= 2; ++y)
        {
            float2 offset = float2(x, y) * uvStepSize;
            resultColor += gTexture.Sample(gSampler, input.texcoord + offset).rgb;
            totalWeight += 1.0f;
        }
    }

    if (totalWeight > 0.0f)
    {
        resultColor /= totalWeight;
    }

    return float4(resultColor, 1.0f);
}