#include "Common.hlsli"
#include "RaymarchFunctions.hlsli"
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
    float3 spotScattering = 0.0f;
    float transmittance = 1.0f;
    float jitter = 0.5f;
    if (temporalControls.z >= 0) {
        uint seed = uint(input.position.x) + uint(input.position.y) * (width / 2);
        seed ^= seed >> 16; seed *= 0x7feb352du; seed ^= seed >> 15; seed *= 0x846ca68bu; seed ^= seed >> 16;
        jitter = frac((float(seed >> 9) + 0.5f) / 8388608.0f + float(uint(temporalControls.z) & 63u) * 0.618033989f);
    }
    for (int index = 0; index < samples; ++index) {
        float travel = (index + jitter) * stepLength;
        float3 relativePosition = ray * travel;
        float density = DensityAt(relativePosition);
        if (density <= 0.0f) { continue; }
        float segmentTransmittance = exp(-density * stepLength);
        if (settings.w != 0.0f && lightColorAndIntensity.w > 0.0f) {
            float3 worldPosition = cameraAndDistance.xyz + relativePosition;
            scattering += ShadowVisibility(worldPosition) * UnderwaterLightTransmission(worldPosition) *
                transmittance * (1.0f - segmentTransmittance);
        }
        if (spotCountAndBias.x > 0.0f) {
            spotScattering += SpotScattering(relativePosition, ray, g) * transmittance * (1.0f - segmentTransmittance);
        }
        transmittance *= segmentTransmittance;
        if (transmittance < 0.001f) { break; }
    }
    float3 color = (scattering * phase * lightColorAndIntensity.rgb * lightColorAndIntensity.w + spotScattering) * temporalControls.w;
    RaymarchOutput output;
    output.scatteringAndDepth = float4(clamp(color, 0.0f, 8.0f), surfaceDistance);
    output.transmittance = 1.0f;
    // 既存のライトだけの設定では、背景を減衰させない。
    if (fogColorAndEnabled.w != 0.0f) { output.transmittance = saturate(transmittance); }
    return output;
}
