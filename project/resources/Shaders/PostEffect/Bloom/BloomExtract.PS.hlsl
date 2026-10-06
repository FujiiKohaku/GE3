#include "Bloom.hlsli"
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    float3 color = max(gTexture.Sample(gSampler, input.texcoord).rgb, 0.0f);
    float brightness = max(color.r, max(color.g, color.b));
    float knee = max(threshold * 0.25f, 0.0001f);
    float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee);
    float contribution = max(brightness - threshold, soft) / max(brightness, 0.0001f);
    if (bloomEnabled == 0) { contribution = 0.0f; }
    return float4(color * contribution, 1.0f);
}
