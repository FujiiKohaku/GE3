#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

float32_t CalcImpactFlashMask(float32_t2 centeredTexcoord)
{
    float32_t radius = length(centeredTexcoord);
    float32_t center = saturate(1.0f - radius * 1.55f);
    float32_t ring = saturate(1.0f - abs(radius - 0.58f) * 13.0f);
    float32_t blade = saturate(1.0f - abs(centeredTexcoord.y) * 8.0f);
    float32_t bladeLength = saturate(1.0f - abs(centeredTexcoord.x) * 0.70f);
    float32_t sharpBlade = blade * bladeLength;

    return saturate(center + ring * 0.70f + sharpBlade);
}

float32_t CalcImpactEdgeMask(float32_t2 centeredTexcoord)
{
    float32_t radius = length(centeredTexcoord);
    float32_t edge = saturate(1.0f - abs(radius - 0.82f) * 18.0f);
    float32_t thinBlade = saturate(1.0f - abs(centeredTexcoord.y) * 16.0f);
    float32_t thinLength = saturate(1.0f - abs(centeredTexcoord.x) * 0.95f);

    return saturate(edge + thinBlade * thinLength);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 baseColor = ShadeParticleBase(input);
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t2 centeredTexcoord = texcoord * 2.0f - 1.0f;

    float32_t flashMask = CalcImpactFlashMask(centeredTexcoord);
    float32_t edgeMask = CalcImpactEdgeMask(centeredTexcoord);

    float32_t3 whiteColor = float32_t3(1.0f, 1.0f, 0.92f);
    float32_t3 yellowColor = float32_t3(1.0f, 0.74f, 0.12f);
    float32_t3 redColor = float32_t3(1.0f, 0.08f, 0.0f);

    float32_t4 color;
    color.rgb = whiteColor * flashMask;
    color.rgb += yellowColor * flashMask * 0.75f;
    color.rgb += redColor * edgeMask * 0.90f;
    color.rgb = floor(color.rgb * 5.0f) / 5.0f;
    color.a = baseColor.a * saturate(flashMask + edgeMask * 0.65f);

    if (color.a <= 0.01f)
    {
        discard;
    }

    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
