#ifndef RETICLE_STYLE_HLSLI
#define RETICLE_STYLE_HLSLI

static const float kReticleStrokeWidthPixels = 1.1f;
static const float kReticleOutlineWidthPixels = 1.2f;
static const float3 kReticleColor = float3(0.95f, 0.97f, 1.0f);
static const float3 kReticleReadyColor = float3(0.325f, 0.847f, 0.910f);
static const float3 kReticleTrackColor = float3(0.20f, 0.27f, 0.32f);

float ReticleStrokeMask(float distancePixels)
{
    return 1.0f - smoothstep(kReticleStrokeWidthPixels,
        kReticleStrokeWidthPixels + 0.8f, distancePixels);
}

float ReticleOutlineMask(float distancePixels)
{
    return 1.0f - smoothstep(kReticleStrokeWidthPixels + kReticleOutlineWidthPixels,
        kReticleStrokeWidthPixels + kReticleOutlineWidthPixels + 0.8f, distancePixels);
}

float4 ComposeReticleColor(float3 color, float strokeAlpha, float outlineAlpha)
{
    float alpha = strokeAlpha + outlineAlpha * (1.0f - strokeAlpha);
    if (alpha > 0.0f) {
        color *= strokeAlpha / alpha;
    }
    return float4(color, alpha);
}

#endif
