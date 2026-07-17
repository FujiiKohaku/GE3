#include "Fullscreen.hlsli"
Texture2D<float4> gTexture : register(t0); SamplerState gSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_TARGET
{
    uint w, h; gTexture.GetDimensions(w, h); float2 texel = 1.0f / float2(w, h);
    float4 base = gTexture.Sample(gSampler, input.texcoord); float3 glare = 0.0f;
    float2 dirs[4] = { float2(1,0), float2(0,1), normalize(float2(1,1)), normalize(float2(1,-1)) };
    for (int d = 0; d < 4; ++d) for (int i = 1; i <= 12; ++i) {
        float2 off = dirs[d] * texel * float(i) * lightRadius * 40.0f;
        float3 a = gTexture.Sample(gSampler, saturate(input.texcoord + off)).rgb;
        float3 b = gTexture.Sample(gSampler, saturate(input.texcoord - off)).rgb;
        glare += (a + b) * smoothstep(lightThreshold, lightThreshold + 0.5f, max(max(a.r,a.g),a.b)) / float(i);
    }
    return float4(saturate(base.rgb + glare * lightStrength * 0.035f), base.a);
}
