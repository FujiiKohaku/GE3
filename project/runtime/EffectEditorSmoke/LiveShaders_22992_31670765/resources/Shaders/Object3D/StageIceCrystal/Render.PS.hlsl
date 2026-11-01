#include "../StageIceCommon.PS.hlsli"

StageIcePixelOutput main(VertexShaderOutput input)
{
    float4 textureColor = SampleStageIceTexture(input);
    float3 surfaceColor = ShadeStageIceStructure(input, textureColor.rgb,
        float3(0.78f, 0.94f, 1.0f), 0.11f);
    float3 normal = normalize(input.normal);
    float3 viewDirection = normalize(gCamera.worldPosition - input.worldPosition);
    float rim = 1.0f - saturate(dot(normal, viewDirection));
    float heightBlend = saturate(input.worldPosition.y * 0.025f);
    const float3 kCoreGlow = float3(0.18f, 0.68f, 1.25f);
    const float3 kTipGlow = float3(0.58f, 0.30f, 1.10f);
    float3 emission = lerp(kCoreGlow, kTipGlow, heightBlend) * (0.45f + rim * rim * 1.4f);
    surfaceColor += emission;
    // Emission remains in the non-directional component during RT shadow composition.
    indirectLightingColor += emission;
    return FinishStageIce(input, surfaceColor, textureColor.a);
}
