#include "Common.hlsli"
float4 main(PixelInput input) : SV_TARGET
{
    float4 scene = sceneTexture.SampleLevel(linearSampler, input.texcoord, 0);
    int2 pixel = GetPixel(input.texcoord);
    float depth = depthTexture.Load(int3(pixel, 0));
    float4 normal = normalTexture.Load(int3(pixel, 0));
    float4 material = materialTexture.Load(int3(pixel, 0));
    float3 indirectColor = 0;
    if (IsValidSurface(depth, normal, material)) {
        uint width, height;
        indirectTexture.GetDimensions(width, height);
        float2 position = input.texcoord * float2(width, height) - 0.5f;
        int2 basePixel = int2(floor(position));
        float2 fraction = frac(position);
        float3 worldNormal = GetWorldNormal(normal);
        float3 viewNormal = mul(float4(worldNormal, 0), view).xyz;
        float3 surfacePosition = Reconstruct(input.texcoord, depth);
        if (dot(viewNormal, -surfacePosition) < 0) { worldNormal = -worldNormal; }
        float4 centerMetadata = float4(worldNormal, surfacePosition.z);
        float weightSum = 0;
        for (int offsetY = 0; offsetY < 2; ++offsetY) {
            for (int offsetX = 0; offsetX < 2; ++offsetX) {
                int2 neighborPixel = clamp(basePixel + int2(offsetX, offsetY), int2(0, 0), int2(width, height) - 1);
                float4 neighborMetadata = metadataTexture.Load(int3(neighborPixel, 0));
                float2 weights = 1 - fraction;
                if (offsetX == 1) { weights.x = fraction.x; }
                if (offsetY == 1) { weights.y = fraction.y; }
                float weight = weights.x * weights.y * GetGeometryWeight(centerMetadata, neighborMetadata);
                indirectColor += indirectTexture.Load(int3(neighborPixel, 0)).rgb * weight;
                weightSum += weight;
            }
        }
        if (weightSum > 0) { indirectColor /= weightSum; }
    }
    if (composition.y > 0.5f) { return float4(indirectColor * composition.x, 1); }
    return float4(scene.rgb + indirectColor * composition.x, scene.a);
}
