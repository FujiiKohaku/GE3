#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t4 textureColor = SampleParticleTexture(texcoord);
    float smokeMask = smoothstep(0.02f, 0.72f, textureColor.r);
    float32_t4 color = gMaterial.color * input.color;
    color.a *= smokeMask;
    if (color.a <= gMaterial.alphaReference)
    {
        discard;
    }
    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
