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
float4 main(VertexShaderOutput input) : SV_Target0 {
    uint width, height; gDepth.GetDimensions(width, height);
    int2 pixel = int2(input.position.xy) * 2;
    float depth = 1.0f;
    // Conservative depth prevents foreground geometry from receiving light from behind it.
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
    for (int index = 0; index < samples; ++index) {
        float travel = (index + 0.5f) * stepLength;
        float3 samplePosition = cameraAndDistance.xyz + ray * travel;
        float transmittance = exp(-lightDirectionAndDensity.w * travel);
        scattering += ShadowVisibility(samplePosition) * transmittance * lightDirectionAndDensity.w * stepLength;
    }
    float3 color = scattering * phase * lightColorAndIntensity.rgb * lightColorAndIntensity.w;
    return float4(clamp(color, 0.0f, 8.0f), surfaceDistance);
}
