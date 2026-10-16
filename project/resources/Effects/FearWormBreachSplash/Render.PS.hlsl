#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"
PixelShaderOutput main(VertexShaderOutput input)
{
    float2 offset = GetParticleTextureCoordinate(input) * 2.0f - 1.0f;
    float4 color = gMaterial.color * input.color;
    color.a *= 1.0f - smoothstep(0.25f, 1.0f, dot(offset, offset));
    clip(color.a - 0.003f);
    return MakeParticlePixelOutput(ApplyParticleFog(color, input));
}
