#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 color = ShadeParticleBase(input);
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t2 centeredTexcoord = texcoord * 2.0f - 1.0f;

    float32_t radius = length(centeredTexcoord);
    float32_t center = saturate(1.0f - radius * 1.35f);
    float32_t ring = saturate(1.0f - abs(radius - 0.62f) * 12.0f);
    float32_t slashA = saturate(1.0f - abs(centeredTexcoord.x - centeredTexcoord.y) * 6.0f);
    float32_t slashB = saturate(1.0f - abs(centeredTexcoord.x + centeredTexcoord.y) * 6.0f);
    float32_t slash = max(slashA, slashB);

    float32_t3 hotColor = float32_t3(1.0f, 0.92f, 0.45f);
    float32_t3 magentaColor = float32_t3(0.95f, 0.18f, 1.0f);
    float32_t3 redColor = float32_t3(1.0f, 0.04f, 0.0f);

    color.rgb = hotColor * center;
    color.rgb += magentaColor * ring * 1.10f;
    color.rgb += redColor * slash * 0.65f;
    color.rgb = saturate(color.rgb);
    color.rgb = lerp(floor(color.rgb * 3.0f) / 3.0f, color.rgb, 0.18f);
    color.a *= saturate(center + ring * 0.80f + slash * 0.45f);

    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
