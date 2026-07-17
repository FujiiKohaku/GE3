#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); SamplerState gSampler : register(s0); static const float PI2=6.28318530718f;
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 base=gTexture.Sample(gSampler,input.texcoord); float3 halo=0.0f;
    for(int i=0;i<20;++i){float a=PI2*float(i)/20.0f;float2 uv=input.texcoord+float2(cos(a),sin(a))*lightRadius;float3 c=gTexture.Sample(gSampler,saturate(uv)).rgb;halo+=c*smoothstep(lightThreshold,lightThreshold+0.5f,max(max(c.r,c.g),c.b));}
    return float4(saturate(base.rgb+halo/20.0f*lightStrength),base.a);
}
