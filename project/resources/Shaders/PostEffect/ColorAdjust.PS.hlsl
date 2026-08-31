#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 outputColor = gTexture.Sample(gSampler, input.texcoord);

    float3 adjustedColor = outputColor.rgb + colorBrightness;
    adjustedColor = (adjustedColor - 0.5f) * colorContrast + 0.5f;

    float luminance = dot(adjustedColor, float3(0.2125f, 0.7154f, 0.0721f));
    adjustedColor = lerp(float3(luminance, luminance, luminance), adjustedColor, colorSaturation);

    outputColor.rgb = saturate(adjustedColor);
    return outputColor;
}
