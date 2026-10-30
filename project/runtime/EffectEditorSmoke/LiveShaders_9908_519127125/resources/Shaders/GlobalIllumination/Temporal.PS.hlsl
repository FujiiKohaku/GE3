#include "Common.hlsli"
IndirectOutput main(PixelInput input)
{
    uint width, height;
    indirectTexture.GetDimensions(width, height);
    int2 pixel = clamp(int2(input.position.xy), int2(0, 0), int2(width, height) - 1);
    IndirectOutput output;
    output.indirectColor = indirectTexture.Load(int3(pixel, 0));
    output.metadata = metadataTexture.Load(int3(pixel, 0));
    if (temporal.x < 0.5f || output.metadata.w <= 0) { return output; }
    float2 motion = motionTexture.Load(int3(GetPixel(input.texcoord), 0)) + temporal.zw;
    float2 historyUv = input.texcoord - motion;
    float2 halfTexel = 0.5f / float2(width, height);
    if (any(historyUv < halfTexel) || any(historyUv > 1 - halfTexel)) { return output; }
    int2 historyPixel = clamp(int2(historyUv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
    float4 previousMetadata = historyMetadataTexture.Load(int3(historyPixel, 0));
    if (previousMetadata.w <= 0 || dot(previousMetadata.xyz, output.metadata.xyz) < 0.95f) { return output; }
    float3 previousPosition = mul(float4(ReconstructLinear(input.texcoord, output.metadata.w), 1), currentToPreviousView).xyz;
    if (previousPosition.z <= 0 || abs(previousMetadata.w - previousPosition.z) > max(0.05f, previousPosition.z * 0.01f)) { return output; }
    float4 previousClip = mul(float4(previousPosition, 1), previousProjection);
    float2 expectedUv = previousClip.xy / previousClip.w * float2(0.5f, -0.5f) + 0.5f;
    uint screenWidth, screenHeight;
    depthTexture.GetDimensions(screenWidth, screenHeight);
    float2 screenSize = float2(screenWidth, screenHeight);
    if (any(abs(expectedUv - historyUv) * screenSize > 3)) { return output; }
    float3 minimumColor = output.indirectColor.rgb;
    float3 maximumColor = output.indirectColor.rgb;
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            int2 neighborPixel = clamp(pixel + int2(offsetX, offsetY), int2(0, 0), int2(width, height) - 1);
            float4 neighborMetadata = metadataTexture.Load(int3(neighborPixel, 0));
            if (GetGeometryWeight(output.metadata, neighborMetadata) > 0.1f) {
                float3 color = indirectTexture.Load(int3(neighborPixel, 0)).rgb;
                minimumColor = min(minimumColor, color);
                maximumColor = max(maximumColor, color);
            }
        }
    }
    float3 previousColor = historyTexture.SampleLevel(linearSampler, historyUv, 0).rgb;
    previousColor = clamp(previousColor, minimumColor, maximumColor);
    float historyWeight = temporal.y * (1 - saturate(length(motion * screenSize) / 24));
    output.indirectColor.rgb = lerp(output.indirectColor.rgb, previousColor, historyWeight);
    return output;
}
