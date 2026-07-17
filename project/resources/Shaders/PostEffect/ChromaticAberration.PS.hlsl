#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 direction = input.texcoord - float2(0.5f, 0.5f);
    float2 offset = direction * chromaticAberrationStrength;

    float4 centerColor = gTexture.Sample(gSampler, input.texcoord);
    float red = gTexture.Sample(gSampler, saturate(input.texcoord + offset)).r;
    float blue = gTexture.Sample(gSampler, saturate(input.texcoord - offset)).b;

    return float4(red, centerColor.g, blue, centerColor.a);
}
