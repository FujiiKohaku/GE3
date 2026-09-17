#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t2 centeredTexcoord = texcoord * 2.0f - 1.0f;
    float32_t burstMask = AnimeStarburstMask(centeredTexcoord);
    float32_t coreMask = AnimeDiamondMask(centeredTexcoord, 0.28f, 0.08f);
    float32_t4 color = gMaterial.color * input.color;
    color.rgb = QuantizeAnimeParticleColor(color.rgb);
    color.rgb = lerp(color.rgb, float32_t3(1.0f, 0.94f, 0.78f), coreMask * 0.82f);
    color.a *= burstMask;
    if (color.a <= gMaterial.alphaReference)
    {
        discard;
    }
    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
