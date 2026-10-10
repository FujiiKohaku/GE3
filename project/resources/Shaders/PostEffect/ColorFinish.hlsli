#include "HdrColor.hlsli"
Texture2D<float> exposureTexture : register(t4);
float HdrShoulderCurve(float value) {
    return (value * (2.43f * value + 0.03f)) / (value * (2.43f * value + 0.59f) + 0.14f);
}
float3 FinishColor(float3 hdr) {
    float exposure = max(toneExposure, 0.0f) * exp2(clamp(colorFinishSettings.z, -10, 10));
    if (colorFinishSettings.y > 0.5f) { exposure *= exposureTexture.Load(int3(0, 0, 0)); }
    float3 color = SanitizeHdr(SanitizeHdr(hdr) * exposure);
    if (toneMapEnabled != 0) {
        if (colorFinishSettings.x > 0.5f) {
            // Compress brightness with one scale, preserving linear RGB ratios.
            float maximum = max(color.r, max(color.g, color.b));
            if (maximum > 0) { color *= HdrShoulderCurve(maximum) / maximum; }
        } else {
            color = (color * (2.51f * color + 0.03f)) / (color * (2.43f * color + 0.59f) + 0.14f);
        }
    }
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(luma.xxx, color, toneSaturation);
    return saturate((color - 0.18f) * toneContrast + 0.18f);
}
