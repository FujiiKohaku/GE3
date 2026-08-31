#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); SamplerState gSampler : register(s0);
float Luma(float3 c) { return dot(c, float3(0.2126f, 0.7152f, 0.0722f)); }
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 base = gTexture.Sample(gSampler, input.texcoord);
    float2 axis = radialBlurCenter - input.texcoord;
    float3 flare = 0.0f;
    for (int i = 1; i <= 5; ++i) {
        float2 uv = saturate(input.texcoord + axis * (float(i) * 0.32f));
        float3 c = gTexture.Sample(gSampler, uv).rgb;
        flare += c * smoothstep(lightThreshold, lightThreshold + 0.4f, Luma(c)) / float(i);
    }
    float ring = 1.0f - smoothstep(0.015f, 0.045f, abs(length(input.texcoord - radialBlurCenter) - lightRadius));
    return float4(saturate(base.rgb + flare * lightStrength * 0.35f + ring * float3(1.0f, 0.55f, 0.2f) * 0.25f), base.a);
}
