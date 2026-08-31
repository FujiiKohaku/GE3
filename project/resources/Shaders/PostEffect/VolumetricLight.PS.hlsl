#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); Texture2D<float> gDepthTexture : register(t1); SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 base = gTexture.Sample(gSampler, input.texcoord); float2 stepUV = (radialBlurCenter - input.texcoord) / 24.0f;
    float2 uv = input.texcoord; float3 volume = 0.0f; float transmission = 1.0f;
    for (int i = 0; i < 24; ++i) { uv += stepUV; float depth = gDepthTexture.Sample(gSampler, saturate(uv)); float3 c = gTexture.Sample(gSampler, saturate(uv)).rgb; float bright = smoothstep(lightThreshold, lightThreshold + 0.4f, max(max(c.r,c.g),c.b)); volume += c * bright * transmission * depth; transmission *= 0.92f; }
    return float4(saturate(base.rgb + volume * lightStrength * 0.045f), base.a);
}
