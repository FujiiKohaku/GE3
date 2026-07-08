#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float RandomFromIndex(float value)
{
    return frac(sin(value * 12.9898f) * 43758.5453f);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    uint width;
    uint height;
    gTexture.GetDimensions(width, height);

    float aspect = (float) width / (float) height;
    float2 center = radialBlurCenter;
    float2 toPixel = input.texcoord - center;
    float2 aspectDirection = float2(toPixel.x * aspect, toPixel.y);
    float distanceFromCenter = length(aspectDirection);

    static const float kPI = 3.14159265f;
    static const float kTwoPI = 6.28318530f;
    static const float kLineCount = 52.0f;

    float angle = atan2(aspectDirection.y, aspectDirection.x);
    float normalizedAngle = (angle + kPI) / kTwoPI;
    float lineIndex = floor(normalizedAngle * kLineCount);
    float randomStrength = RandomFromIndex(lineIndex + 1.0f);

    float linePhase = normalizedAngle * kLineCount + time * 1.35f + randomStrength * 0.5f;
    float linePattern = abs(frac(linePhase) - 0.5f) * 2.0f;
    float linePatternBlurA = abs(frac(linePhase + 0.08f) - 0.5f) * 2.0f;
    float linePatternBlurB = abs(frac(linePhase - 0.08f) - 0.5f) * 2.0f;
    float lineShape = 1.0f - smoothstep(0.0f, 0.24f, linePattern);
    float lineShapeBlurA = 1.0f - smoothstep(0.0f, 0.32f, linePatternBlurA);
    float lineShapeBlurB = 1.0f - smoothstep(0.0f, 0.32f, linePatternBlurB);
    lineShape = lineShape * 0.50f + lineShapeBlurA * 0.25f + lineShapeBlurB * 0.25f;
    lineShape = pow(lineShape, 1.15f);

    float centerFade = smoothstep(0.18f, 0.35f, distanceFromCenter);
    float outerFade = 1.0f - smoothstep(0.95f, 1.20f, distanceFromCenter);
    float radialMask = centerFade * outerFade;

    float segment = frac(distanceFromCenter * 5.4f - time * 2.5f + randomStrength);
    float segmentStart = smoothstep(0.0f, 0.16f, segment);
    float segmentEnd = 1.0f - smoothstep(0.42f, 0.58f, segment);
    float segmentMask = segmentStart * segmentEnd;

    float lineStrength = lineShape * radialMask;
    lineStrength *= 0.18f + randomStrength * 0.32f;
    lineStrength *= 0.10f + segmentMask * 0.48f;
    lineStrength *= 1.0f + boostKickStrength * 1.25f;

    float3 lineColor = float3(0.62f, 0.82f, 1.0f);
    float3 darkenedColor = textureColor.rgb * (1.0f - lineStrength * 0.06f);
    float3 outputColor = darkenedColor + lineColor * lineStrength * 0.38f;

    textureColor.rgb = saturate(outputColor);
    return textureColor;
}
