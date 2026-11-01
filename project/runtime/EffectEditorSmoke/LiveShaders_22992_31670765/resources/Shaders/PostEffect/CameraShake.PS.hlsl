#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float ShakeHash(float value)
{
    return frac(sin(value * 91.3458f) * 47453.5453f);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float frame = floor(time * 30.0f);
    float2 randomOffset;
    randomOffset.x = ShakeHash(frame + 1.0f) * 2.0f - 1.0f;
    randomOffset.y = ShakeHash(frame + 17.0f) * 2.0f - 1.0f;

    float2 sampleUV = saturate(input.texcoord + randomOffset * cameraShakeStrength);
    return gTexture.Sample(gSampler, sampleUV);
}
