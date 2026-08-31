#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint w,h;gTexture.GetDimensions(w,h);float2 t=1.0f/float2(w,h);float4 base=gTexture.Sample(gSampler,input.texcoord);float3 dx=abs(gTexture.Sample(gSampler,input.texcoord+t*float2(1,0)).rgb-gTexture.Sample(gSampler,input.texcoord-t*float2(1,0)).rgb);float3 dy=abs(gTexture.Sample(gSampler,input.texcoord+t*float2(0,1)).rgb-gTexture.Sample(gSampler,input.texcoord-t*float2(0,1)).rgb);float edge=saturate(length(dx+dy)*3.0f);float3 neon=lerp(float3(0,0.8f,1),float3(1,0,0.8f),input.texcoord.y);return float4(saturate(base.rgb+neon*edge*lightStrength),base.a);
}
