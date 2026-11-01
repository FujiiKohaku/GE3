#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float2 texcoord = GetParticleTextureCoordinate(input);
    float2 centeredTexcoord = texcoord * 2.0f - 1.0f;
    float coverage = 1.0f - smoothstep(0.1f, 1.0f, length(centeredTexcoord));
    float4 color = SampleParticleTexture(texcoord) * gMaterial.color * input.color;
    color.a *= coverage;
    if (color.a <= gMaterial.alphaReference) { discard; }
    color = ApplyParticleFog(color, input);
    return MakeParticlePixelOutput(color);
}
