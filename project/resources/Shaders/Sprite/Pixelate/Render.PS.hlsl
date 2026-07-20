#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput { float4 color : SV_Target0; };

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float pixelCount = max(gEffect.amplitude, 2.0f);
    float2 uv = floor(input.texcoord * pixelCount) / pixelCount;
    uv = mul(float4(uv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    output.color = gTexture.Sample(gSampler, uv) * gMaterial.color;
    return output;
}
