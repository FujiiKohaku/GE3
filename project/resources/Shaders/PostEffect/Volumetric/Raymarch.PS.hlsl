#include "Common.hlsli"
Texture2D<float> gShadow : register(t2);
SamplerComparisonState gShadowSampler : register(s1);
float ShadowVisibility(float3 worldPosition) {
    float4 clip = mul(float4(worldPosition, 1.0f), lightViewProjection);
    if (clip.w <= 0.000001f) { return 0.0f; }
    float3 projected = clip.xyz / clip.w;
    float2 uv = projected.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv <= 0.0f) || any(uv >= 1.0f) || projected.z <= 0.0f || projected.z >= 1.0f) { return 0.0f; }
    // Do not assume unshadowed light outside the shadow camera's supported volume.
    float edge = min(min(uv.x, 1.0f - uv.x), min(uv.y, 1.0f - uv.y));
    float fade = smoothstep(0.0f, 0.08f, edge) * smoothstep(0.0f, 0.04f, min(projected.z, 1.0f - projected.z));
    return fade * gShadow.SampleCmpLevelZero(gShadowSampler, uv, projected.z - settings.z);
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
struct RaymarchOutput {
    float4 scatteringAndDepth : SV_Target0;
    float transmittance : SV_Target1;
};
RaymarchOutput main(VertexShaderOutput input) {
    uint width, height; gDepth.GetDimensions(width, height);
    int2 pixel = int2(input.position.xy) * 2;
    float depth = 1.0f;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            depth = min(depth, gDepth.Load(int3(min(pixel + int2(x, y), int2(width - 1, height - 1)), 0)));
        }
    }
    float3 delta = ReconstructRelative(input.texcoord, depth);
    float surfaceDistance = min(length(delta), 60000.0f);
    float3 ray = delta / max(surfaceDistance, 0.000001f);
    float distance = min(surfaceDistance, cameraAndDistance.w);
    int samples = clamp(int(settings.y), 8, 64);
    float stepLength = distance / samples;
    float g = clamp(settings.x, -0.8f, 0.8f);
    float cosAngle = dot(ray, -lightDirectionAndDensity.xyz);
    float phase = (1.0f - g * g) / pow(max(1.0f + g * g - 2.0f * g * cosAngle, 0.04f), 1.5f);
    float scattering = 0.0f;
    float transmittance = 1.0f;
    for (int index = 0; index < samples; ++index) {
        float travel = (index + 0.5f) * stepLength;
        float3 relativePosition = ray * travel;
        float density = DensityAt(relativePosition);
        if (density <= 0.0f) { continue; }
        float segmentTransmittance = exp(-density * stepLength);
        if (settings.w != 0.0f && lightColorAndIntensity.w > 0.0f) {
            scattering += ShadowVisibility(cameraAndDistance.xyz + relativePosition) * transmittance * (1.0f - segmentTransmittance);
        }
        transmittance *= segmentTransmittance;
        if (transmittance < 0.001f) { break; }
    }
    float3 color = scattering * phase * lightColorAndIntensity.rgb * lightColorAndIntensity.w;
    RaymarchOutput output;
    output.scatteringAndDepth = float4(clamp(color, 0.0f, 8.0f), surfaceDistance);
    output.transmittance = 1.0f;
    // 既存のライトだけの設定では、背景を減衰させない。
    if (fogColorAndEnabled.w != 0.0f) { output.transmittance = saturate(transmittance); }
    return output;
}
