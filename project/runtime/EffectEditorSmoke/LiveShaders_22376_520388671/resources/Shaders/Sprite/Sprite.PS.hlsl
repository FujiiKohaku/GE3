#include "Sprite.hlsli"

// PS—p Material2D ‚Í b1
ConstantBuffer<Material2D> gMaterial : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV =
        mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);

    float4 textureColor =
        gTexture.Sample(gSampler, transformedUV.xy);

    output.color = gMaterial.color * textureColor;

    return output;
}
