#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float2 texcoord = GetParticleTextureCoordinate(input);
    // Ring UV.y runs across the band rather than across a square sprite.
    float bandDistance = abs(texcoord.y - 0.5f) * 2.0f;
    float coverage = 1.0f - smoothstep(0.0f, 0.8f, bandDistance);
    float4 color = SampleParticleTexture(texcoord) * gMaterial.color * input.color;
    color.a *= coverage;
    if (color.a <= gMaterial.alphaReference) { discard; }
    color = ApplyParticleFog(color, input);
    return MakeParticlePixelOutput(color);
}
