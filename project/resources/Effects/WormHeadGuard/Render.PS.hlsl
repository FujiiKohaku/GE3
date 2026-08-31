#include "../Common/ParticlePixelCommon.hlsli"
#include "../Common/ParticleFogCommon.hlsli"

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t4 baseColor = ShadeParticleBase(input);
    float32_t2 texcoord = GetParticleTextureCoordinate(input);
    float32_t2 centeredTexcoord = texcoord * 2.0f - 1.0f;

    float32_t radius = length(centeredTexcoord);
    float32_t shieldRing = saturate(1.0f - abs(radius - 0.62f) * 15.0f);
    float32_t innerRing = saturate(1.0f - abs(radius - 0.38f) * 18.0f);
    float32_t horizontal = saturate(1.0f - abs(centeredTexcoord.y) * 12.0f);
    float32_t horizontalLength = saturate(1.0f - abs(centeredTexcoord.x) * 0.75f);
    float32_t animeLine = horizontal * horizontalLength;

    float32_t3 whiteColor = float32_t3(0.90f, 1.0f, 1.0f);
    float32_t3 blueColor = float32_t3(0.05f, 0.55f, 1.0f);
    float32_t3 deepBlueColor = float32_t3(0.02f, 0.08f, 1.0f);

    float32_t4 color;
    color.rgb = whiteColor * innerRing * 0.85f;
    color.rgb += blueColor * shieldRing * 1.25f;
    color.rgb += deepBlueColor * animeLine * 0.80f;
    color.rgb = saturate(color.rgb);
    color.rgb = lerp(floor(color.rgb * 3.0f) / 3.0f, color.rgb, 0.20f);
    color.a = baseColor.a * saturate(shieldRing + innerRing * 0.60f + animeLine * 0.70f);

    if (color.a <= 0.01f)
    {
        discard;
    }

    color = ApplyParticleFog(color, input);

    return MakeParticlePixelOutput(color);
}
