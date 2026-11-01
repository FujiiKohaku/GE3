#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

float GetArcDistance(float2 position, float radius, float startAngle, float endAngle)
{
    float angle = clamp(atan2(position.x, position.y), startAngle, endAngle);
    float2 arcPosition = float2(sin(angle), cos(angle)) * radius;
    return length(position - arcPosition);
}

PixelShaderOutput main(SpriteVertexOutput input)
{
    const float kRadiusPixels = 56.0f;
    const float kHalfArcAngle = 1.1f;
    const float kBorderHalfWidthPixels = 6.0f;
    const float kTrackHalfWidthPixels = 4.5f;
    const float kFillHalfWidthPixels = 3.5f;
    float2 position = (input.texcoord - 0.5f) * gEffect.spriteSize;
    float arcDistance = GetArcDistance(position, kRadiusPixels, -kHalfArcAngle, kHalfArcAngle);
    float featherPixels = max(fwidth(arcDistance), 0.75f);
    float borderCoverage = 1.0f - smoothstep(
        kBorderHalfWidthPixels - featherPixels, kBorderHalfWidthPixels + featherPixels, arcDistance);
    if (borderCoverage <= 0.0f) { discard; }

    float trackCoverage = 1.0f - smoothstep(
        kTrackHalfWidthPixels - featherPixels, kTrackHalfWidthPixels + featherPixels, arcDistance);
    float3 color = lerp(float3(0.65f, 0.72f, 0.76f), float3(0.015f, 0.025f, 0.04f), trackCoverage);
    float hpRatio = saturate(gEffect.threshold);
    if (hpRatio > 0.0f) {
        float fillEndAngle = lerp(-kHalfArcAngle, kHalfArcAngle, hpRatio);
        float fillDistance = GetArcDistance(position, kRadiusPixels, -kHalfArcAngle, fillEndAngle);
        float fillCoverage = 1.0f - smoothstep(
            kFillHalfWidthPixels - featherPixels, kFillHalfWidthPixels + featherPixels, fillDistance);
        color = lerp(color, gMaterial.color.rgb, fillCoverage);
    }

    PixelShaderOutput output;
    output.color = float4(color, borderCoverage * 0.95f);
    return output;
}
