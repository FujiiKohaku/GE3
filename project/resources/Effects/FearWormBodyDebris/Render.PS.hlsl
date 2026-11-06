#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"
PixelShaderOutput main(VertexShaderOutput input)
{
    float4 color = gMaterial.color * input.color;
    float faceLight = 0.68f + 0.32f * abs(normalize(input.normal).y);
    color.rgb *= faceLight;
    clip(color.a - 0.003f);
    return MakeParticlePixelOutput(ApplyParticleFog(color, input));
}
