#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float GrainHash(float2 value)
{
    return frac(sin(dot(value, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint width;
    uint height;
    gTexture.GetDimensions(width, height);

    float4 color = gTexture.Sample(gSampler, input.texcoord);
    float frame = floor(time * 60.0f);
    float grain = GrainHash(input.texcoord * float2(width, height) + frame) - 0.5f;
    color.rgb = saturate(color.rgb + grain * filmGrainStrength);
    return color;
}
