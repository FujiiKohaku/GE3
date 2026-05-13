#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } },
};

static const float kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};

static const float kPrewittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

float Luminance(float3 color)
{
    return dot(color, float3(0.2125f, 0.7154f, 0.0721f));
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width;
    uint height;

    gTexture.GetDimensions(width, height);

    float2 uvStepSize;
    uvStepSize.x = 1.0f / float(width);
    uvStepSize.y = 1.0f / float(height);

    float2 difference = float2(0.0f, 0.0f);

    for (int x = 0; x < 3; x++)
    {

        for (int y = 0; y < 3; y++)
        {

            float2 texcoord = input.texcoord + kIndex3x3[x][y] * uvStepSize;

            float3 fetchColor = gTexture.Sample(gSampler, texcoord).rgb;
            float luminance = Luminance(fetchColor);

            difference.x += luminance * kPrewittHorizontalKernel[x][y];
            difference.y += luminance * kPrewittVerticalKernel[x][y];
        }
    }

    float weight = length(difference);
    weight = saturate(weight * 6.0f);

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    float4 outputColor;
    outputColor.rgb = (1.0f - weight) * textureColor.rgb;
    outputColor.a = textureColor.a;

    return outputColor;
}