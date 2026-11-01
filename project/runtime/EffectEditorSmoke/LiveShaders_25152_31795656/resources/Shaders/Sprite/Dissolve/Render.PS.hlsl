#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput { float4 color : SV_Target0; };

float Hash21(float2 value)
{
    value = frac(value * float2(123.34f, 456.21f));
    value += dot(value, value + 45.32f);
    return frac(value.x * value.y);
}

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 color = gTexture.Sample(gSampler, uv) * gMaterial.color;
    float threshold = 0.5f + 0.45f * sin(gFrame.elapsedTime * gEffect.speed);
    float noise = Hash21(floor(input.texcoord * gEffect.frequency));
    clip(noise - threshold);
    float edge = 1.0f - smoothstep(threshold, threshold + 0.12f, noise);
    color.rgb = lerp(color.rgb, float3(1.0f, 0.45f, 0.05f), edge);
    output.color = color;
    return output;
}
