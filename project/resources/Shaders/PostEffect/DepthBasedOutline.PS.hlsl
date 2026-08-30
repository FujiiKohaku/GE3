#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
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

// Standard perspective depth is nonlinear. Recover view-space Z before comparing it.
float RestoreViewDepth(float depth)
{
    float nearClip = max(outlineNearClip, 0.0001f);
    float farClip = max(outlineFarClip, nearClip + 0.0001f);
    return nearClip * farClip / max(farClip - saturate(depth) * (farClip - nearClip), 0.0001f);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width;
    uint height;

    gDepthTexture.GetDimensions(width, height);
    int2 lastPixel = int2(width, height) - 1;
    int2 centerPixel = clamp(int2(input.texcoord * float2(width, height)), int2(0, 0), lastPixel);

    float2 difference = float2(0.0f, 0.0f);
    float nearestDepth = max(outlineFarClip, 0.0001f);

    for (int x = 0; x < 3; x++)
    {
        for (int y = 0; y < 3; y++)
        {
            // Point loads avoid blending foreground/background depths before reconstruction.
            int2 pixel = clamp(centerPixel + int2(kIndex3x3[x][y]), int2(0, 0), lastPixel);
            float depth = RestoreViewDepth(gDepthTexture.Load(int3(pixel, 0)));
            nearestDepth = min(nearestDepth, depth);

            difference.x += depth * kPrewittHorizontalKernel[x][y];
            difference.y += depth * kPrewittVerticalKernel[x][y];
        }
    }

    // The same relative depth step now has the same strength near and far.
    float relativeDifference = length(difference) / max(nearestDepth, 0.0001f);
    float weight = smoothstep(outlineThreshold,
        outlineThreshold + max(outlineSoftness, 0.0001f), relativeDifference);

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    float4 outputColor;
    outputColor.rgb = (1.0f - weight) * textureColor.rgb;
    outputColor.a = textureColor.a;

    return outputColor;
}
