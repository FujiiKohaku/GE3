#ifndef EFFECT_PARTICLE_PIXEL_COMMON_HLSLI
#define EFFECT_PARTICLE_PIXEL_COMMON_HLSLI

#include "ParticleCommon.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
SamplerState gSampler : register(s0);
Texture2D<float32_t4> gTexture : register(t1);

float32_t2 GetParticleTextureCoordinate(VertexShaderOutput input)
{
    float32_t2 texcoord = input.texcoord;
    texcoord.y = 1.0f - texcoord.y;

    return texcoord;
}

float32_t4 SampleParticleTexture(float32_t2 texcoord)
{
    float32_t4 uv = mul(float32_t4(texcoord, 0.0f, 1.0f), gMaterial.uvTransform);

    return gTexture.Sample(gSampler, uv.xy);
}

float32_t4 ShadeParticleBase(VertexShaderOutput input)
{
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t4 textureColor = SampleParticleTexture(texcoord);
    float32_t4 color = gMaterial.color * textureColor * input.color;

    if (color.a <= gMaterial.alphaReference)
    {
        discard;
    }

    return color;
}

PixelShaderOutput MakeParticlePixelOutput(float32_t4 color)
{
    PixelShaderOutput output;
    output.color = color;

    return output;
}

#endif
