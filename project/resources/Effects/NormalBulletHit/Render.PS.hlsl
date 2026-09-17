#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

float32_t CalcAnimeImpactMask(float32_t2 centeredTexcoord)
{
    float32_t radius = length(centeredTexcoord);
    float32_t centerFlash = saturate(1.0f - radius * 1.45f);
    float32_t outerRing = saturate(1.0f - abs(radius - 0.56f) * 11.0f);
    float32_t diagonalLineA = saturate(1.0f - abs(centeredTexcoord.x - centeredTexcoord.y) * 5.5f);
    float32_t diagonalLineB = saturate(1.0f - abs(centeredTexcoord.x + centeredTexcoord.y) * 5.5f);
    float32_t slashLine = max(diagonalLineA, diagonalLineB);

    return saturate(centerFlash + outerRing * 0.75f + slashLine * 0.45f);
}

float32_t CalcAnimeOutline(float32_t2 centeredTexcoord)
{
    float32_t radius = length(centeredTexcoord);
    float32_t outline = saturate(1.0f - abs(radius - 0.74f) * 16.0f);

    return outline;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 color = ShadeParticleBase(input);
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t2 centeredTexcoord = texcoord * 2.0f - 1.0f;

    float32_t impactMask = CalcAnimeImpactMask(centeredTexcoord);
    float32_t outlineMask = CalcAnimeOutline(centeredTexcoord);
    float32_t celMask = saturate(impactMask * 1.25f + outlineMask * 0.65f);

    float32_t3 hotColor = float32_t3(0.92f, 1.0f, 1.0f);
    float32_t3 sparkColor = float32_t3(0.20f, 0.88f, 1.0f);
    float32_t3 inkColor = float32_t3(0.02f, 0.24f, 0.58f);

    color.rgb = lerp(color.rgb, hotColor, saturate(celMask * 0.72f));
    color.rgb += sparkColor * impactMask * 1.25f;
    color.rgb += inkColor * outlineMask * 1.10f;
    color.rgb = saturate(color.rgb);
    color.rgb = QuantizeAnimeParticleColor(color.rgb);

    color.a *= saturate(0.50f + impactMask * 1.20f + outlineMask * 0.85f);
    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
