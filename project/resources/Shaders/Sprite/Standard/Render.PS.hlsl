#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(
        float4(input.texcoord, 0.0f, 1.0f),
        gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    output.color = gMaterial.color * textureColor;
    return output;
}
