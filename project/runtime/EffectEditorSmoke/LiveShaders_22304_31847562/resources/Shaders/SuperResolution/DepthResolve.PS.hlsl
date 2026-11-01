#include "../PostEffect/Fullscreen.hlsli"
Texture2D<float> sceneDepth : register(t1);
cbuffer DepthResolveConstants : register(b0) { float4 sourceJitterUv; }
float main(VertexShaderOutput input) : SV_Depth {
    uint width, height; sceneDepth.GetDimensions(width, height);
    float2 sourcePixel = (input.texcoord + sourceJitterUv.xy) * float2(width, height);
    // Nearest source sample avoids interpolating foreground/background depths.
    int2 pixel = clamp(int2(floor(sourcePixel)), int2(0, 0), int2(width - 1, height - 1));
    return sceneDepth.Load(int3(pixel, 0));
}
