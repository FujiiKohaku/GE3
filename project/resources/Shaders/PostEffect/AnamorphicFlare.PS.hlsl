#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint w, h; gTexture.GetDimensions(w,h); float4 base = gTexture.Sample(gSampler,input.texcoord); float3 flare = 0.0f;
    for (int i = -24; i <= 24; ++i) { float2 uv = input.texcoord + float2(float(i) / float(w) * lightRadius * 20.0f, 0); float3 c = gTexture.Sample(gSampler,saturate(uv)).rgb; flare += c * smoothstep(lightThreshold,lightThreshold+0.5f,max(max(c.r,c.g),c.b)) / (1.0f + abs(float(i))); }
    return float4(saturate(base.rgb + flare * float3(0.25f,0.55f,1.0f) * lightStrength * 0.12f),base.a);
}
