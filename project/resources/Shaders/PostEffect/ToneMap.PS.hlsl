#include "Fullscreen.hlsli"
#include "ColorFinish.hlsli"
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.SampleLevel(gSampler, input.texcoord, 0.0f);
    return float4(FinishColor(color.rgb), color.a);
}
