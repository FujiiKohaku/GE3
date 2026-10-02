#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 color = ShadeParticleBase(input);
    // Emissive cards retain radiance above one for HDR bloom.
    color.rgb *= 2.2f;
    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
