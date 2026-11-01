#ifndef KOHAKU_VOLUME_FUNCTIONS
#define KOHAKU_VOLUME_FUNCTIONS
#ifndef VOLUME_SHADOW_REGISTER
#define VOLUME_SHADOW_REGISTER t2
#endif
#ifndef VOLUME_LOCAL_SHADOW_REGISTER
#define VOLUME_LOCAL_SHADOW_REGISTER t4
#endif
Texture2D<float> gVolumeShadow : register(VOLUME_SHADOW_REGISTER);
Texture2DArray<float> gVolumeLocalShadows : register(VOLUME_LOCAL_SHADOW_REGISTER);
SamplerComparisonState gVolumeShadowSampler : register(s1);
float SpotShadowVisibility(VolumetricSpotConstants light, float3 relativePosition) {
    int shadowFace = int(light.decayAndFalloffAndShadow.z);
    if (shadowFace < 0) { return 1.0f; }
    float4 clip = mul(float4(cameraAndDistance.xyz + relativePosition, 1.0f), light.lightViewProjection);
    if (clip.w <= 0.000001f) { return 0.0f; }
    float3 projected = clip.xyz / clip.w;
    float2 uv = projected.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv <= 0.0f) || any(uv >= 1.0f) || projected.z <= 0.0f || projected.z >= 1.0f) { return 0.0f; }
    return gVolumeLocalShadows.SampleCmpLevelZero(gVolumeShadowSampler,
        float3(uv, shadowFace), projected.z - spotCountAndBias.y);
}
float3 SpotScattering(float3 relativePosition, float3 ray, float anisotropy) {
    float3 radiance = 0.0f;
    for (int lightIndex = 0; lightIndex < int(spotCountAndBias.x); ++lightIndex) {
        VolumetricSpotConstants light = spotLights[lightIndex];
        float3 delta = relativePosition - light.positionAndDistance.xyz;
        float distance = length(delta);
        if (distance < 0.00001f || distance >= light.positionAndDistance.w) { continue; }
        float3 direction = delta / distance;
        float cone = saturate((dot(direction, light.directionAndCosAngle.xyz) - light.directionAndCosAngle.w) /
            max(light.decayAndFalloffAndShadow.y - light.directionAndCosAngle.w, 0.001f));
        if (cone <= 0.0f) { continue; }
        float attenuation = pow(saturate(1.0f - distance / light.positionAndDistance.w), light.decayAndFalloffAndShadow.x);
        float cosAngle = dot(ray, -direction);
        float phase = (1.0f - anisotropy * anisotropy) /
            pow(max(1.0f + anisotropy * anisotropy - 2.0f * anisotropy * cosAngle, 0.04f), 1.5f);
        radiance += light.colorAndIntensity.rgb * light.colorAndIntensity.w * cone * attenuation * phase *
            SpotShadowVisibility(light, relativePosition);
    }
    return radiance;
}
// Broad moving windows at the water surface create shafts aligned with the sun.
float UnderwaterLightTransmission(float3 worldPosition) {
    if (underwaterSettings.y <= 0.0f) { return 1.0f; }
    float waterDepth = max(underwaterSettings.x - worldPosition.y, 0.0f);
    float2 surfacePosition = worldPosition.xz - lightDirectionAndDensity.xz *
        waterDepth / max(-lightDirectionAndDensity.y, 0.2f);
    float elapsedSeconds = underwaterSettings.z;
    float window = sin(surfacePosition.x * 0.055f + surfacePosition.y * 0.031f + elapsedSeconds * 0.08f);
    window += sin(surfacePosition.y * 0.064f - surfacePosition.x * 0.023f - elapsedSeconds * 0.06f) * 0.55f;
    float shafts = 0.18f + smoothstep(-0.25f, 0.95f, window) * 1.25f;
    return lerp(1.0f, shafts, underwaterSettings.y);
}
float ShadowVisibility(float3 worldPosition) {
    float4 clip = mul(float4(worldPosition, 1.0f), lightViewProjection);
    if (clip.w <= 0.000001f) { return 0.0f; }
    float3 projected = clip.xyz / clip.w;
    float2 uv = projected.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv <= 0.0f) || any(uv >= 1.0f) || projected.z <= 0.0f || projected.z >= 1.0f) { return 0.0f; }
    // Do not assume unshadowed light outside the shadow camera's supported volume.
    float edge = min(min(uv.x, 1.0f - uv.x), min(uv.y, 1.0f - uv.y));
    float fade = smoothstep(0.0f, 0.08f, edge) * smoothstep(0.0f, 0.04f, min(projected.z, 1.0f - projected.z));
    return fade * gVolumeShadow.SampleCmpLevelZero(gVolumeShadowSampler, uv, projected.z - settings.z);
}
// 周期256の格子ノイズ。時間で乱数を変えず、空間内で滑らかに補間する。
float NoiseHash(float3 cell) {
    cell = cell - floor(cell / 256.0f) * 256.0f;
    return frac(sin(dot(cell, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
}
float SmoothNoise(float3 position) {
    float3 cell = floor(position);
    float3 fraction = frac(position);
    fraction = fraction * fraction * (3.0f - 2.0f * fraction);
    float lower = lerp(lerp(NoiseHash(cell), NoiseHash(cell + float3(1, 0, 0)), fraction.x),
        lerp(NoiseHash(cell + float3(0, 1, 0)), NoiseHash(cell + float3(1, 1, 0)), fraction.x), fraction.y);
    float upper = lerp(lerp(NoiseHash(cell + float3(0, 0, 1)), NoiseHash(cell + float3(1, 0, 1)), fraction.x),
        lerp(NoiseHash(cell + float3(0, 1, 1)), NoiseHash(cell + float3(1, 1, 1)), fraction.x), fraction.y);
    return lerp(lower, upper, fraction.z);
}
float DensityAt(float3 relativePosition) {
    float density = lightDirectionAndDensity.w;
    if (fogColorAndEnabled.w == 0.0f) { return density; }
    density = 0.0f;
    if (heightAndVolumeCount.y > 0.0f) {
        float height = relativePosition.y + cameraAndDistance.y;
        density += heightAndVolumeCount.y * exp(-max(height - heightAndVolumeCount.x, 0.0f) * heightAndVolumeCount.z);
    }
    for (int volumeIndex = 0; volumeIndex < int(heightAndVolumeCount.w); ++volumeIndex) {
        FogVolumeConstants volume = volumes[volumeIndex];
        float3 delta = relativePosition - volume.centerAndShape.xyz;
        float insideDistance;
        if (volume.centerAndShape.w == 0.0f) {
            insideDistance = volume.radiusAndSoftness.x - length(delta);
        } else {
            float3 remaining = volume.extentsAndDensity.xyz - abs(delta);
            insideDistance = min(remaining.x, min(remaining.y, remaining.z));
        }
        density += volume.extentsAndDensity.w * smoothstep(0.0f, volume.radiusAndSoftness.y, insideDistance);
    }
    if (density > 0.0f && noiseSettings.x > 0.0f) {
        float3 noisePosition = (relativePosition + cameraAndDistance.xyz) * noiseScaleAndOffset.x - noiseScaleAndOffset.yzw;
        density *= lerp(1.0f, SmoothNoise(noisePosition), noiseSettings.x);
    }
    return clamp(density, 0.0f, 0.1f);
}

#endif
