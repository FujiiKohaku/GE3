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
    float32_t diagonalA = saturate(1.0f - abs(centeredTexcoord.x - centeredTexcoord.y) * 8.5f);
    float32_t diagonalB = saturate(1.0f - abs(centeredTexcoord.x + centeredTexcoord.y) * 8.5f);
    float32_t diagonal = max(diagonalA, diagonalB) * saturate(1.0f - radius * 0.45f);

    return saturate(center * 1.15f + ring * 0.85f + sharpBlade + diagonal * 0.55f);
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
    float32_t3 yellowColor = float32_t3(1.0f, 0.82f, 0.16f);
    float32_t3 redColor = float32_t3(1.0f, 0.08f, 0.0f);

    float32_t4 color;
    color.rgb = whiteColor * flashMask * 1.15f;
    color.rgb += yellowColor * flashMask * 0.95f;
    color.rgb += redColor * edgeMask * 1.15f;
    color.rgb = saturate(color.rgb);
    color.rgb = lerp(floor(color.rgb * 3.0f) / 3.0f, color.rgb, 0.15f);
    color.a = baseColor.a * saturate(flashMask * 1.20f + edgeMask * 0.85f);

    if (color.a <= 0.01f)
    {
        discard;
    }

    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
