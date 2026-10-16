// Final pass: x = eased heat, y = onset flash, z = cooling (-1) / recovery (0..1).
float HeatFlameHash(float2 position)
{
    float3 hashValue = frac(float3(position.xyx) * 0.1031f);
    hashValue += dot(hashValue, hashValue.yzx + 33.33f);
    return frac((hashValue.x + hashValue.y) * hashValue.z);
}

float HeatFlameNoise(float2 position)
{
    float2 cell = floor(position);
    float2 fraction = frac(position);
    float2 blend = fraction * fraction * (3.0f - 2.0f * fraction);
    float bottom = lerp(HeatFlameHash(cell), HeatFlameHash(cell + float2(1.0f, 0.0f)), blend.x);
    float top = lerp(HeatFlameHash(cell + float2(0.0f, 1.0f)),
        HeatFlameHash(cell + float2(1.0f, 1.0f)), blend.x);
    return lerp(bottom, top, blend.y);
}

float CalculateHeatLeak(float2 uv, float heatIntensity)
{
    float2 centerDistance = abs(uv - 0.5f) * 2.0f;
    float centerClear = smoothstep(0.36f, 0.86f, max(centerDistance.x, centerDistance.y));
    if (centerClear <= 0.0f)
    {
        return 0.0f;
    }
    // Positive texture motion in screen Y makes the flame pattern travel upward.
    float flow = HeatFlameNoise(uv * float2(18.0f, 7.0f) + float2(0.0f, time * 1.6f));
    float detail = HeatFlameNoise(uv * float2(39.0f, 14.0f) + float2(2.7f, time * 2.7f));
    float flame = flow * 0.7f + detail * 0.3f;
    float flickerStrength = lerp(0.3f, 1.0f, heatIntensity);
    float2 flameUv = uv + float2((flow - 0.5f) * 0.045f,
        (detail - 0.5f) * 0.055f) * flickerStrength;
    // Light sources sit just inside the border so their visible cores are not clipped.
    float width = lerp(0.075f, 0.15f, heatIntensity);
    float leftHeight = 0.72f + sin(time * 0.72f) * 0.045f;
    float rightHeight = 0.43f + sin(time * 0.57f + 1.8f) * 0.055f;
    float topPosition = 0.58f + sin(time * 0.61f + 0.8f) * 0.045f;
    float bottomPosition = 0.42f + sin(time * 0.67f + 2.4f) * 0.04f;
    float2 leftOffset = (flameUv - float2(0.015f, leftHeight)) / float2(width, 0.28f);
    float2 rightOffset = (flameUv - float2(0.985f, rightHeight)) / float2(width * 0.94f, 0.24f);
    float2 topOffset = (flameUv - float2(topPosition, 0.015f)) / float2(0.28f, width);
    float2 lowOffset = (flameUv - float2(bottomPosition, 0.985f)) / float2(0.28f, width);
    float lightLeak = exp2(-dot(leftOffset, leftOffset) * 2.0f) +
        exp2(-dot(rightOffset, rightOffset) * 2.0f) * 0.85f +
        exp2(-dot(topOffset, topOffset) * 2.0f) * 0.85f +
        exp2(-dot(lowOffset, lowOffset) * 2.0f) * 0.85f;
    float flameBrightness = lerp(1.0f, lerp(0.50f, 1.45f, smoothstep(0.20f, 0.80f, flame)), flickerStrength);
    return saturate(lightLeak * centerClear * flameBrightness);
}

float2 CalculateHeatUv(float2 uv)
{
    float heatIntensity = saturate(customParameter0);
    if (heatIntensity <= 0.0f)
    {
        return uv;
    }
    float edgeMask = CalculateHeatLeak(uv, heatIntensity);
    float wave = sin(uv.y * 47.0f - time * 3.0f + sin(uv.x * 19.0f + time));
    float crossWave = sin(uv.x * 39.0f + uv.y * 13.0f - time * 2.2f);
    float distortion = edgeMask * heatIntensity * heatIntensity;
    return saturate(uv + float2(wave * 0.0018f, crossWave * 0.0009f) * distortion);
}

float4 ApplyHeatEdge(float4 color, float2 uv)
{
    float heatIntensity = saturate(customParameter0);
    float onsetFlash = saturate(customParameter1);
    float recoveryFlash = saturate(customParameter2);
    if (max(heatIntensity, max(onsetFlash, recoveryFlash)) <= 0.0f)
    {
        return color;
    }
    float lightSpread = max(heatIntensity, max(onsetFlash, recoveryFlash));
    float edgeMask = CalculateHeatLeak(uv, lightSpread);
    if (edgeMask <= 0.0f)
    {
        return color;
    }
    float nearLimit = smoothstep(0.70f, 1.0f, heatIntensity);
    float breathing = 1.0f + sin(time * 3.6f) * nearLimit * 0.16f;
    float plume = HeatFlameNoise(uv * float2(27.0f, 11.0f) + float2(7.3f, time * 2.0f));
    float flameCore = smoothstep(0.35f, 0.82f, plume);
    const float3 kFlameEdgeColor = float3(1.0f, 0.10f, 0.004f);
    const float3 kFlameCoreColor = float3(1.0f, 0.32f, 0.015f);
    float3 heatLight = lerp(kFlameEdgeColor, kFlameCoreColor, flameCore);
    float3 glow = heatLight * edgeMask * heatIntensity * breathing * (1.0f + flameCore * 0.18f);

    // A single soft flash peaks immediately and fades quickly, without a moving ring.
    float hotFlash = onsetFlash * onsetFlash * onsetFlash;
    glow += float3(1.0f, 0.48f, 0.055f) * hotFlash * edgeMask * 0.95f;

    if (customParameter2 < 0.0f)
    {
        // Thin rising wisps accompany cooling, confined to the same outer mask.
        float steam = plume * (0.5f + 0.5f * sin(uv.y * 29.0f + time * 3.1f));
        glow += float3(0.25f, 0.10f, 0.025f) * steam * edgeMask * heatIntensity;
    }
    if (recoveryFlash > 0.0f)
    {
        glow += float3(0.45f, 0.83f, 1.0f) * edgeMask * recoveryFlash * recoveryFlash * 0.72f;
    }
    // Screen blending lifts light without laying an opaque colored film over the scene.
    color.rgb = 1.0f - (1.0f - color.rgb) * (1.0f - saturate(glow));
    return color;
}
