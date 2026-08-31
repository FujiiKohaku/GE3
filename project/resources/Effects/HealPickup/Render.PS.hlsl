#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 color = ShadeParticleBase(input);
    color = ApplyParticleFog(color, input);
    return MakeParticlePixelOutput(color);
}
