#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float kKernel3x3[3][3] =
{
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
};

static const float2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } },
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width;
    uint height;

    gTexture.GetDimensions(width, height);

    float2 uvStepSize;

    uvStepSize.x = 1.0f / float(width);
    uvStepSize.y = 1.0f / float(height);

    float3 resultColor = float3(0.0f, 0.0f, 0.0f);

    for (int x = 0; x < 3; x++)
    {

        for (int y = 0; y < 3; y++)
        {

            float2 texcoord =
                input.texcoord +
                kIndex3x3[x][y] * uvStepSize;

            float3 fetchColor =
                gTexture.Sample(gSampler, texcoord).rgb;

            resultColor +=
                fetchColor * kKernel3x3[x][y];
        }
    }

    float4 outputColor;

    outputColor.rgb = resultColor;
    outputColor.a = 1.0f;

    return outputColor;
}