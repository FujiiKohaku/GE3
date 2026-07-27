#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float2 uv = GetParticleTextureCoordinate(input);
    float4 textureColor = SampleParticleTexture(uv);
    float mask = max(textureColor.r, textureColor.a);
    float glow = smoothstep(0.01f, 0.55f, mask);
    float3 deepBlue = float3(0.0f, 0.06f, 1.0f);
    float4 color = float4(
        deepBlue * 2.4f,
        glow * input.color.a * 0.72f);
    if (color.a <= gMaterial.alphaReference) discard;
    color = ApplyParticleFog(color, input);
    return MakeParticlePixelOutput(color);
}
