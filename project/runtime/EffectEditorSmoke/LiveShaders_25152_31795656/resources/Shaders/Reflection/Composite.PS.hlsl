#include "Common.hlsli"
float4 UpsampleReflection(float2 uv, float4 encoded)
{
    uint width, height;
    reflectionTexture.GetDimensions(width, height);
    float2 position = uv * float2(width, height) - 0.5f;
    int2 basePixel = int2(floor(position));
    float2 fraction = frac(position);
    float centerDepth = Reconstruct(uv, depthTexture.Load(int3(GetPixel(uv), 0))).z;
    float3 centerNormal = normalize(encoded.xyz * 2 - 1);
    float4 sum = 0;
    float weightSum = 0;
    for (int offsetY = 0; offsetY < 2; ++offsetY) {
        for (int offsetX = 0; offsetX < 2; ++offsetX) {
            int2 pixel = clamp(basePixel + int2(offsetX, offsetY), int2(0, 0), int2(width, height) - 1);
            float2 sampleUv = (float2(pixel) + 0.5f) / float2(width, height);
            float4 sampleNormal = normalTexture.Load(int3(GetPixel(sampleUv), 0));
            if (sampleNormal.a < 2 || dot(centerNormal, normalize(sampleNormal.xyz * 2 - 1)) < 0.98f) { continue; }
            float sampleDepth = Reconstruct(sampleUv, depthTexture.Load(int3(GetPixel(sampleUv), 0))).z;
            if (abs(sampleDepth - centerDepth) > max(0.1f, centerDepth * 0.02f)) { continue; }
            float2 weights = 1 - fraction;
            if (offsetX == 1) { weights.x = fraction.x; }
            if (offsetY == 1) { weights.y = fraction.y; }
            float weight = weights.x * weights.y;
            sum += reflectionTexture.Load(int3(pixel, 0)) * weight;
            weightSum += weight;
        }
    }
    if (weightSum <= 0.00001f) { return 0; }
    return sum / weightSum;
}
float4 main(PixelInput input) : SV_TARGET
{
    float4 reflection = reflectionTexture.SampleLevel(linearSampler, input.texcoord, 0);
    if (settings.w >= 1.5f) { return float4(reflection.rgb, 1); }
    float4 encoded = normalTexture.Load(int3(GetPixel(input.texcoord), 0));
    if (encoded.a >= 2) { reflection = UpsampleReflection(input.texcoord, encoded); }
    else { reflection = 0; }
    if (settings.w > 0.5f) {
        if (reflection.a <= 0.00001f) { return float4(0, 0, 0, 1); }
        return float4(reflection.rgb / reflection.a, 1);
    }
    float4 scene = sceneTexture.SampleLevel(linearSampler, input.texcoord, 0);
    if (encoded.a < 2.0f || reflection.a <= 0.00001f) { return scene; }
    float3 reflectionColor = reflection.rgb / reflection.a;
    float reflectionWeight = saturate(reflection.a * settings.z);
    // Keep the directly lit glossy floor underneath the reflected radiance.
    scene.rgb = scene.rgb * (1 - reflectionWeight * 0.35f) + reflectionColor * reflectionWeight;
    return scene;
}
