#include "Bloom.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 sceneColor = gTexture.Sample(gSampler, input.texcoord);

    float luminance = dot(
        sceneColor.rgb,
        float3(0.2126f, 0.7152f, 0.0722f));

    float3 outputColor = float3(0.0f, 0.0f, 0.0f);

    if (bloomEnabled != 0) {
        if (luminance > threshold) {
            outputColor = sceneColor.rgb;
        }
    }

    return float4(outputColor, 1.0f);
}
