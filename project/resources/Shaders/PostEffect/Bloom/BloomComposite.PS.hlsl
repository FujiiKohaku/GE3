#include "Bloom.hlsli"

Texture2D<float4> gSceneTexture : register(t0);
Texture2D<float4> gBloomTexture : register(t1);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 sceneColor = gSceneTexture.Sample(gSampler, input.texcoord);
    float3 outputColor = sceneColor.rgb;

    if (bloomEnabled != 0) {
        float3 bloomColor = gBloomTexture.Sample(gSampler, input.texcoord).rgb;
        outputColor = sceneColor.rgb + bloomColor * intensity;
    }

    return float4(saturate(outputColor), sceneColor.a);
}
