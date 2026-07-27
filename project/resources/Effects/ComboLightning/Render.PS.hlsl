#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float2 uv = GetParticleTextureCoordinate(input);
    float4 textureColor = SampleParticleTexture(uv);
    float mask = max(textureColor.r, textureColor.a);
    float glow = smoothstep(0.02f, 0.48f, mask);
    float core = smoothstep(0.58f, 0.92f, mask);
    float3 electricBlue = float3(0.01f, 0.12f, 1.0f);
    float3 coreBlue = float3(0.25f, 0.72f, 1.0f);
    float3 colorRgb = lerp(electricBlue, coreBlue, core);
    float4 color = float4(
        colorRgb * (1.8f + core * 3.2f),
        glow * input.color.a);
    if (color.a <= gMaterial.alphaReference) discard;
    color = ApplyParticleFog(color, input);
    return MakeParticlePixelOutput(color);
}
