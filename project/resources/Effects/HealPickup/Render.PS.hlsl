#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 color = ShadeParticleBase(input);
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t2 centeredTexcoord = texcoord * 2.0f - 1.0f;
    float32_t crossMask = AnimeCrossMask(centeredTexcoord, 0.15f, 0.78f);
    float32_t coreMask = AnimeDiamondMask(centeredTexcoord, 0.30f, 0.08f);
    color.rgb = QuantizeAnimeParticleColor(color.rgb);
    color.rgb = lerp(color.rgb, float32_t3(0.92f, 1.0f, 0.96f), coreMask * 0.75f);
    color.a *= crossMask;
    if (color.a <= gMaterial.alphaReference)
    {
        discard;
    }
    color = ApplyParticleFog(color, input);
    return MakeParticlePixelOutput(color);
}
