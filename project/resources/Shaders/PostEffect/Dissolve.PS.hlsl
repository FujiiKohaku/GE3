#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float mask = gMaskTexture.Sample(gSampler, input.texcoord).r;

    float activeThreshold = dissolveThreshold;
    if (animationEnabled != 0) { activeThreshold = 0.5f + sin(time) * 0.5f; }
    if (mask <= activeThreshold)
    {
        return float4(0.0f, 1.0f, 0.0f, 1.0f);
    }

    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    float edge = 1.0f - smoothstep(activeThreshold, activeThreshold + dissolveEdgeWidth, mask);

    textureColor.rgb += edge * dissolveEdgeStrength * float3(1.0f, 0.4f, 0.0f);

    return textureColor;
}
