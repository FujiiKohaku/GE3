#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float DirtHash(float2 value)
{
    return frac(sin(dot(value, float2(127.1f, 311.7f))) * 43758.5453f);
}

float DirtNoise(float2 value)
{
    float2 cell = floor(value);
    float2 local = frac(value);
    local = local * local * (3.0f - 2.0f * local);

    float bottom = lerp(DirtHash(cell), DirtHash(cell + float2(1.0f, 0.0f)), local.x);
    float top = lerp(DirtHash(cell + float2(0.0f, 1.0f)), DirtHash(cell + float2(1.0f, 1.0f)), local.x);
    return lerp(bottom, top, local.y);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, input.texcoord);
    float dirt = DirtNoise(input.texcoord * 7.0f);
    dirt *= DirtNoise(input.texcoord * 23.0f + 4.7f);
    dirt = smoothstep(0.32f, 0.72f, dirt);

    float luminance = dot(color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
    float lightResponse = smoothstep(0.55f, 1.0f, luminance);
    float3 dirtColor = float3(1.0f, 0.82f, 0.58f);
    color.rgb = saturate(color.rgb + dirtColor * dirt * lightResponse * lensDirtStrength);
    return color;
}
