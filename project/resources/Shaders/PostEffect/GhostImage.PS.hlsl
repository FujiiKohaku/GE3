#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 base=gTexture.Sample(gSampler,input.texcoord);float2 mirrored=1.0f-input.texcoord;float3 ghost=0.0f;
    for(int i=1;i<=4;++i){float scale=0.55f+float(i)*0.13f;float2 uv=(mirrored-0.5f)*scale+0.5f;float3 c=gTexture.Sample(gSampler,saturate(uv)).rgb;ghost+=c*smoothstep(lightThreshold,lightThreshold+0.4f,max(max(c.r,c.g),c.b))/float(i);}
    return float4(saturate(base.rgb+ghost*lightStrength*0.22f),base.a);
}
