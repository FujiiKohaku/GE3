#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 9x9 ガウシアンフィルタカーネル重み
static const float kWeights[9] = {
    0.051f, 0.092f, 0.122f, 0.135f, 0.150f, 0.135f, 0.122f, 0.092f, 0.051f
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    float2 texelSize = float2(1.0f / 1280.0f, 1.0f / 720.0f); // テクセルサイズ

    float4 blurredColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;

    // 2次元ガウシアンブラーフィルタ
    [unroll]
    for (int x = -4; x <= 4; ++x)
    {
        [unroll]
        for (int y = -4; y <= 4; ++y)
        {
            float2 offset = float2(x, y) * texelSize * 1.8f;
            float weight = kWeights[x + 4] * kWeights[y + 4];
            blurredColor += gTexture.Sample(gSampler, uv + offset) * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0f)
    {
        blurredColor /= totalWeight;
    }

    return blurredColor;
}
