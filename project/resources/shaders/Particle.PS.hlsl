#include "Particle.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
SamplerState gSampler : register(s0);
Texture2D<float32_t4> gTexture : register(t1);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float32_t2 texcoord = input.texcoord;

    // V•ûŒü‚ð”½“]
    texcoord.y = 1.0f - texcoord.y;

    float32_t4 uv = mul(float32_t4(texcoord, 0.0f, 1.0f), gMaterial.uvTransform);

    float32_t4 texColor = gTexture.Sample(gSampler, uv.xy);

    output.color = gMaterial.color * texColor * input.color;

    if (output.color.a <= gMaterial.alphaReference)
    {
        discard;
    }

    return output;
}