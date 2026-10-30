#include "Common.hlsli"
Texture2D<float4> sceneTexture : register(t0);
groupshared uint localHistogram[256];
[numthreads(16, 16, 1)]
void main(uint3 pixel : SV_DispatchThreadID, uint index : SV_GroupIndex) {
    localHistogram[index] = 0; GroupMemoryBarrierWithGroupSync();
    if (pixel.x < uint(luminanceRange.z) && pixel.y < uint(luminanceRange.w)) {
        float3 color = sceneTexture.Load(int3(pixel.xy, 0)).rgb;
        float luminance = dot(max(color, 0), float3(0.2126f, 0.7152f, 0.0722f));
        uint bin = 0;
        if (isfinite(luminance) && luminance > 0.000001f) {
            float value = saturate((log2(luminance) - luminanceRange.x) / (luminanceRange.y - luminanceRange.x));
            bin = 1 + uint(value * 254);
        }
        InterlockedAdd(localHistogram[bin], 1);
    }
    GroupMemoryBarrierWithGroupSync();
    if (localHistogram[index] > 0) { InterlockedAdd(histogram[index], localHistogram[index]); }
}
