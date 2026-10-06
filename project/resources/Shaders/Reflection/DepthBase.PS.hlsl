#include "Common.hlsli"
float2 main(PixelInput input) : SV_TARGET
{
    int2 pixel = int2(input.position.xy);
    uint width, height;
    depthTexture.GetDimensions(width, height);
    // Empty cells have an inverted interval; floor surfaces cannot be SSR hits.
    if (pixel.x >= int(width) || pixel.y >= int(height)) { return float2(1.0e20f, 0); }
    float depth = depthTexture.Load(int3(pixel, 0));
    float4 encoded = normalTexture.Load(int3(pixel, 0));
    if (depth >= 0.999999f || encoded.a >= 2 || encoded.a == -1) { return float2(1.0e20f, 0); }
    float viewDepth = Reconstruct((float2(pixel) + 0.5f) / float2(width, height), depth).z;
    return viewDepth.xx;
}
