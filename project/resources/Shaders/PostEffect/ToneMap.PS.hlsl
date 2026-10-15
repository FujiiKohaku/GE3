#include "Fullscreen.hlsli"
#include "ColorFinish.hlsli"
#include "HeatEdge.hlsli"
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.SampleLevel(gSampler, CalculateHeatUv(input.texcoord), 0.0f);
    return ApplyHeatEdge(float4(FinishColor(color.rgb), color.a), input.texcoord);
}
