#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

ConstantBuffer<ParticleRenderParameter> gRenderParameter : register(b2);

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 color = ShadeParticleBase(input);
    color.rgb *= gRenderParameter.emissionStrength;
    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
