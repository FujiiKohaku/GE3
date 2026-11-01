#include "../Object3d.hlsli"
ConstantBuffer<Material> materialParameters : register(b0);
struct EntryCamera { float3 worldPosition; };
ConstantBuffer<EntryCamera> gCamera : register(b2);

float4 ShadeEntrySurface(VertexShaderOutput input)
{
    float elapsedSeconds = materialParameters.color.x;
    float durationSeconds = materialParameters.color.y;
    float2 uv = input.texcoord;
    float fadeOut = 1.0f - smoothstep(durationSeconds - 1.2f, durationSeconds, elapsedSeconds);
    float warningStrength = smoothstep(0.0f, 1.8f, elapsedSeconds)
        * saturate(materialParameters.color.z);
    // 奥の頭から手前の尾まで、曲がった胴体の影を連ねる。
    float silhouette = 0.0f;
    [unroll]
    for (int segmentIndex = 0; segmentIndex < 10; ++segmentIndex) {
        float bodyRate = segmentIndex / 9.0f;
        float2 center = float2(0.5f + sin(bodyRate * 5.2f + elapsedSeconds * 1.4f) * 0.09f,
            0.29f + bodyRate * 0.50f);
        float2 offset = (uv - center) / float2(0.082f - bodyRate * 0.025f, 0.052f);
        silhouette = max(silhouette, 1.0f - smoothstep(0.55f, 1.4f, length(offset)));
    }
    float2 waterOffset = (uv - float2(0.5f, 0.29f)) * float2(1.0f, 1.62f);
    float distanceToBreach = length(waterOffset);
    float rippleMask = 0.0f;
    [unroll]
    for (int rippleIndex = 0; rippleIndex < 3; ++rippleIndex) {
        float rippleAge = elapsedSeconds - 0.45f - rippleIndex * 0.55f;
        float radius = max(rippleAge, 0.0f) * 0.12f;
        float ring = 1.0f - smoothstep(0.006f, 0.016f, abs(distanceToBreach - radius));
        rippleMask = max(rippleMask, ring * step(0.0f, rippleAge) * saturate(1.0f - radius * 1.7f));
    }
    float shadowAlpha = silhouette * warningStrength * 0.76f;
    float rippleAlpha = rippleMask * fadeOut * 0.50f;
    float alpha = shadowAlpha + rippleAlpha * (1.0f - shadowAlpha);
    clip(alpha - 0.002f);
    float3 color = lerp(float3(0.015f, 0.10f, 0.14f), float3(0.65f, 0.94f, 1.0f), rippleAlpha / max(alpha, 0.001f));
    return float4(color, alpha);
}
