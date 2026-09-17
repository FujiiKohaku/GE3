#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
Texture2D<float4> gNormalTexture : register(t2);
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
    float4 centerNormalSample = gNormalTexture.Load(int3(centerPixel, 0));
    float centerRawDepth = gDepthTexture.Load(int3(centerPixel, 0));
    float3 centerNormal = normalize(centerNormalSample.xyz * 2.0f - 1.0f);
    bool centerNormalValid = centerNormalSample.a >= 0.0f &&
        abs(centerNormalSample.a - centerRawDepth) < 0.002f;
    float normalDifference = 0.0f;

    for (int x = 0; x < 3; x++)
    {
        for (int y = 0; y < 3; y++)
        {
            // Point loads avoid blending foreground/background depths before reconstruction.
            int2 pixel = clamp(centerPixel + int2(kIndex3x3[x][y]), int2(0, 0), lastPixel);
            float rawDepth = gDepthTexture.Load(int3(pixel, 0));
            float depth = RestoreViewDepth(rawDepth);
            nearestDepth = min(nearestDepth, depth);

            difference.x += depth * kPrewittHorizontalKernel[x][y];
            difference.y += depth * kPrewittVerticalKernel[x][y];

            float4 normalSample = gNormalTexture.Load(int3(pixel, 0));
            bool neighborNormalValid = normalSample.a >= 0.0f &&
                abs(normalSample.a - rawDepth) < 0.002f;
            if (centerNormalValid && neighborNormalValid)
            {
                float3 neighborNormal = normalize(normalSample.xyz * 2.0f - 1.0f);
                normalDifference = max(
                    normalDifference,
                    1.0f - saturate(dot(centerNormal, neighborNormal)));
            }
        }
    }

    // The same relative depth step now has the same strength near and far.
    float relativeDifference = length(difference) / max(nearestDepth, 0.0001f);
    float depthWeight = smoothstep(outlineThreshold,
        outlineThreshold + max(outlineSoftness, 0.0001f), relativeDifference);
    float normalWeight = smoothstep(outlineNormalThreshold,
        outlineNormalThreshold + max(outlineNormalSoftness, 0.0001f),
        normalDifference) * outlineNormalStrength;
    float weight = max(depthWeight, normalWeight);

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // A deep ink-blue outline keeps the cel look softer and more cohesive
    // than pure black, while retaining strong silhouettes at 720p.
    const float3 outlineColor = float3(0.027f, 0.067f, 0.122f);
    float4 outputColor;
    outputColor.rgb = lerp(textureColor.rgb, outlineColor, weight * 0.94f);
    outputColor.a = textureColor.a;

    return outputColor;
}
