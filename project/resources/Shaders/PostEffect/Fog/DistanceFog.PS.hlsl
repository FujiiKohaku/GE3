#include "../../Common/Fog.hlsli"

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

Texture2D<float4> gSceneColor : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
ConstantBuffer<FogData> gFog : register(b0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 sceneColor = gSceneColor.Sample(gSampler, input.texcoord);
    float depth = gDepthTexture.Sample(gSampler, input.texcoord);

    float3 viewPosition = RestoreFogViewPosition(depth, input.texcoord, gFog);
    float viewDistance = length(viewPosition);

    float fogFactor = 0.0f;
    if (gFog.isEnabled != 0 && gFog.distanceEnabled != 0)
    {
        fogFactor = CalcDistanceFog(viewDistance, gFog.distance);
    }

    return ApplyFog(sceneColor, gFog.color.rgb, fogFactor);
}
