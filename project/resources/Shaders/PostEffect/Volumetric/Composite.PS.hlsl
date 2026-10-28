#include "Common.hlsli"
Texture2D<float4> gVolume : register(t2);
Texture2D<float> gTransmittance : register(t3);
Texture2D<float> gObjectFogMask : register(t7);
cbuffer ObjectFogMaskOptions : register(b1) { uint gHasObjectFogMask; };
float UnderwaterCaustics(float3 worldPosition) {
    const float kMotionSpeedMultiplier = 1.5f;
    const float kDistortionAmplitude = 0.9f;
    float elapsedSeconds = underwaterSettings.z * kMotionSpeedMultiplier;
    float2 position = worldPosition.xz * 0.26f;
    position += float2(sin(position.y * 0.67f + elapsedSeconds * 0.24f),
        sin(position.x * 0.73f - elapsedSeconds * 0.19f)) * kDistortionAmplitude;
    float first = abs(sin(position.x + elapsedSeconds * 0.17f) +
        sin(position.y - elapsedSeconds * 0.13f));
    float second = abs(sin(position.x * 0.74f - position.y * 0.68f - elapsedSeconds * 0.15f) +
        sin(position.y * 0.83f + position.x * 0.55f + elapsedSeconds * 0.11f));
    float ridge = min(first, second);
    float filterWidth = max(fwidth(ridge), 0.035f);
    return 1.0f - smoothstep(0.04f, 0.14f + filterWidth, ridge);
}
float4 main(VertexShaderOutput input) : SV_Target0 {
    float4 scene = gColor.SampleLevel(gPoint, input.texcoord, 0);
    if (gHasObjectFogMask != 0 && gObjectFogMask.SampleLevel(gPoint, input.texcoord, 0) < 0.5f) return scene;
    float depth = gDepth.SampleLevel(gPoint, input.texcoord, 0);
    float3 relativePosition = ReconstructRelative(input.texcoord, depth);
    float surfaceDistance = min(length(relativePosition), 60000.0f);
    if (underwaterSettings.y > 0.0f) {
        float3 worldPosition = relativePosition + cameraAndDistance.xyz;
        float3 surfaceNormal = cross(ddx(relativePosition), ddy(relativePosition));
        surfaceNormal *= rsqrt(max(dot(surfaceNormal, surfaceNormal), 0.000001f));
        float surfaceWeight = smoothstep(0.25f, 0.8f, abs(surfaceNormal.y));
        surfaceWeight *= saturate((underwaterSettings.x - worldPosition.y) * 0.25f);
        if (depth >= 0.99999f) { surfaceWeight = 0.0f; }
        float caustics = UnderwaterCaustics(worldPosition) * surfaceWeight * exp(-surfaceDistance * 0.004f);
        float3 absorption = exp(-float3(0.0030f, 0.0008f, 0.0003f) * surfaceDistance);
        scene.rgb *= lerp(float3(1, 1, 1), absorption * (0.96f + caustics * 0.52f), underwaterSettings.y);
        scene.rgb += float3(0.025f, 0.075f, 0.065f) * caustics * underwaterSettings.y;
    }
    uint width, height; gVolume.GetDimensions(width, height);
    float2 location = input.texcoord * float2(width, height) - 0.5f;
    int2 origin = int2(floor(location));
    float2 fraction = frac(location);
    float3 light = 0.0f; float totalWeight = 0.0f;
    float transmittance = 0.0f;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            int2 pixel = clamp(origin + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            float4 volume = gVolume.Load(int3(pixel, 0));
            float2 axisWeight = 1.0f - fraction;
            if (x != 0) { axisWeight.x = fraction.x; }
            if (y != 0) { axisWeight.y = fraction.y; }
            float weight = axisWeight.x * axisWeight.y * exp(-abs(volume.a - surfaceDistance) / max(0.5f, surfaceDistance * 0.01f));
            light += volume.rgb * weight; totalWeight += weight;
            transmittance += gTransmittance.Load(int3(pixel, 0)) * weight;
        }
    }
    // No compatible depth samples: omit the effect rather than bleed across silhouettes.
    if (totalWeight > 0.0001f) {
        float transmission = saturate(transmittance / totalWeight);
        scene.rgb = scene.rgb * transmission + fogColorAndEnabled.rgb * temporalControls.w * (1.0f - transmission) + light / totalWeight;
    }
    return scene;
}
