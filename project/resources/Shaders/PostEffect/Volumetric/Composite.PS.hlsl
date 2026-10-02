#include "Common.hlsli"
Texture2D<float4> gVolume : register(t2);
float4 main(VertexShaderOutput input) : SV_Target0 {
    float4 scene = gColor.SampleLevel(gPoint, input.texcoord, 0);
    float depth = gDepth.SampleLevel(gPoint, input.texcoord, 0);
    float surfaceDistance = min(length(ReconstructRelative(input.texcoord, depth)), 60000.0f);
    uint width, height; gVolume.GetDimensions(width, height);
    float2 location = input.texcoord * float2(width, height) - 0.5f;
    int2 origin = int2(floor(location));
    float2 fraction = frac(location);
    float3 light = 0.0f; float totalWeight = 0.0f;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            int2 pixel = clamp(origin + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            float4 volume = gVolume.Load(int3(pixel, 0));
            float2 axisWeight = 1.0f - fraction;
            if (x != 0) { axisWeight.x = fraction.x; }
            if (y != 0) { axisWeight.y = fraction.y; }
            float weight = axisWeight.x * axisWeight.y * exp(-abs(volume.a - surfaceDistance) / max(0.5f, surfaceDistance * 0.01f));
            light += volume.rgb * weight; totalWeight += weight;
        }
    }
    // No compatible depth samples: omit the effect rather than bleed across silhouettes.
    if (totalWeight > 0.0001f) { scene.rgb += light / totalWeight; }
    return scene;
}
