#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 base = gTexture.Sample(gSampler, input.texcoord); float2 stepUV = (radialBlurCenter - input.texcoord) / 32.0f;
    float2 uv = input.texcoord; float3 rays = 0.0f; float decay = 1.0f;
    for (int i = 0; i < 32; ++i) { uv += stepUV; float3 c = gTexture.Sample(gSampler, saturate(uv)).rgb; float l = max(max(c.r,c.g),c.b); rays += c * smoothstep(lightThreshold, lightThreshold + 0.4f, l) * decay; decay *= 0.94f; }
    return float4(saturate(base.rgb + rays * lightStrength * 0.035f), base.a);
}
