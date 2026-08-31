#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float angle=lightAngle; if(animationEnabled!=0){angle+=time*0.25f;} float2 dir=float2(cos(angle),sin(angle)); float4 base=gTexture.Sample(gSampler,input.texcoord);float3 streak=0.0f;
    for(int i=-16;i<=16;++i){float2 uv=input.texcoord+dir*float(i)*lightRadius/16.0f;float3 c=gTexture.Sample(gSampler,saturate(uv)).rgb;streak+=c*smoothstep(lightThreshold,lightThreshold+0.5f,max(max(c.r,c.g),c.b))/(1.0f+abs(float(i)));}
    return float4(saturate(base.rgb+streak*lightStrength*0.12f),base.a);
}
