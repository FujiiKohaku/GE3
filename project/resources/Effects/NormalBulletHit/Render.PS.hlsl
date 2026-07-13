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

    float32_t3 hotColor = float32_t3(1.0f, 0.94f, 0.52f);
    float32_t3 sparkColor = float32_t3(1.0f, 0.62f, 0.02f);
    float32_t3 inkColor = float32_t3(1.0f, 0.08f, 0.0f);

    color.rgb = lerp(color.rgb, hotColor, saturate(impactMask * 0.55f));
    color.rgb += sparkColor * impactMask * 0.95f;
    color.rgb += inkColor * outlineMask * 0.85f;
    color.rgb = floor(color.rgb * 4.0f) / 4.0f;

    color.a *= saturate(0.35f + impactMask * 0.95f + outlineMask * 0.55f);
    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
