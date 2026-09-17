#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 color = ShadeParticleBase(input);
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t2 centeredTexcoord = texcoord * 2.0f - 1.0f;
    // Compress the billboard vertically so the boost reads as a compact
    // tapered flame instead of a long screen-facing ribbon.
    float32_t2 compactTexcoord = centeredTexcoord * float32_t2(1.0f, 1.35f);
    float32_t streakMask = AnimeTaperedStreakMask(compactTexcoord);
    float32_t whiteCore = AnimeTaperedStreakMask(compactTexcoord * float32_t2(2.8f, 1.0f));

    color.rgb = QuantizeAnimeParticleColor(color.rgb);
    color.rgb = lerp(color.rgb, float32_t3(0.92f, 1.0f, 1.0f), whiteCore * 0.82f);
    color.a *= streakMask;
    if (color.a <= gMaterial.alphaReference)
    {
        discard;
    }
    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
