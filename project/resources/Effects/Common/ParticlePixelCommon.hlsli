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

    // Anime VFX use a small number of graphic cards. Posterize both the
    // texture coverage and color so each card reads as one deliberate shape.
    float32_t hardCoverage = smoothstep(0.16f, 0.32f, textureColor.a);
    color.a *= hardCoverage;
    color.rgb = floor(saturate(color.rgb) * 3.0f + 0.5f) / 3.0f;

    if (color.a <= gMaterial.alphaReference)
    {
        discard;
    }

    return color;
}

float32_t AnimeDiamondMask(float32_t2 centeredTexcoord, float32_t radius, float32_t feather)
{
    float32_t distanceToCenter = abs(centeredTexcoord.x) + abs(centeredTexcoord.y);
    return 1.0f - smoothstep(radius - feather, radius, distanceToCenter);
}

float32_t AnimeCrossMask(float32_t2 centeredTexcoord, float32_t halfWidth, float32_t halfLength)
{
    float32_t vertical = (1.0f - smoothstep(halfWidth, halfWidth + 0.04f, abs(centeredTexcoord.x))) *
        (1.0f - smoothstep(halfLength, halfLength + 0.04f, abs(centeredTexcoord.y)));
    float32_t horizontal = (1.0f - smoothstep(halfWidth, halfWidth + 0.04f, abs(centeredTexcoord.y))) *
        (1.0f - smoothstep(halfLength, halfLength + 0.04f, abs(centeredTexcoord.x)));
    return max(vertical, horizontal);
}

float32_t AnimeStarburstMask(float32_t2 centeredTexcoord)
{
    float32_t2 diagonal = float32_t2(
        centeredTexcoord.x + centeredTexcoord.y,
        centeredTexcoord.x - centeredTexcoord.y) * 0.70710678f;
    float32_t axisBurst = AnimeCrossMask(centeredTexcoord, 0.075f, 0.92f);
    float32_t diagonalBurst = AnimeCrossMask(diagonal, 0.055f, 0.72f);
    float32_t core = AnimeDiamondMask(centeredTexcoord, 0.58f, 0.10f);
    return saturate(max(axisBurst, diagonalBurst) + core);
}

float32_t AnimeTaperedStreakMask(float32_t2 centeredTexcoord)
{
    float32_t along = centeredTexcoord.y * 0.5f + 0.5f;
    float32_t halfWidth = lerp(0.045f, 0.34f, along);
    float32_t side = 1.0f - smoothstep(halfWidth, halfWidth + 0.055f, abs(centeredTexcoord.x));
    float32_t lengthMask = 1.0f - smoothstep(0.90f, 1.0f, abs(centeredTexcoord.y));
    return side * lengthMask;
}

float32_t3 QuantizeAnimeParticleColor(float32_t3 color)
{
    return floor(saturate(color) * 3.0f + 0.5f) / 3.0f;
}

PixelShaderOutput MakeParticlePixelOutput(float32_t4 color)
{
    PixelShaderOutput output;
    output.color = color;

    return output;
}

#endif
