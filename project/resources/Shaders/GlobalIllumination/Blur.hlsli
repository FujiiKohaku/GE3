#include "Common.hlsli"
float4 BlurIndirect(PixelInput input, int2 direction)
{
    uint width, height;
    indirectTexture.GetDimensions(width, height);
    int2 pixel = clamp(int2(input.position.xy), int2(0, 0), int2(width, height) - 1);
    float4 centerMetadata = metadataTexture.Load(int3(pixel, 0));
    if (centerMetadata.w <= 0) { return 0; }
    float4 sum = 0;
    float weightSum = 0;
    for (int offset = -3; offset <= 3; ++offset) {
        int2 neighborPixel = clamp(pixel + direction * offset, int2(0, 0), int2(width, height) - 1);
        float4 neighborMetadata = metadataTexture.Load(int3(neighborPixel, 0));
        float weight = GetGeometryWeight(centerMetadata, neighborMetadata) * exp(-float(offset * offset) / 4.5f);
        sum += indirectTexture.Load(int3(neighborPixel, 0)) * weight;
        weightSum += weight;
    }
    if (weightSum <= 0) { return 0; }
    return sum / weightSum;
}
