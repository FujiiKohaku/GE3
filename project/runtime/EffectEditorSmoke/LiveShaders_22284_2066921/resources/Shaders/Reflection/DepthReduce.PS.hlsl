#include "Common.hlsli"
float2 main(PixelInput input) : SV_TARGET
{
    int2 pixel = int2(input.position.xy) * 2;
    uint width, height;
    depthPyramid.GetDimensions(width, height);
    float2 bounds = float2(1.0e20f, 0);
    for (int pixelY = 0; pixelY < 2; ++pixelY) {
        for (int pixelX = 0; pixelX < 2; ++pixelX) {
            int2 sourcePixel = min(pixel + int2(pixelX, pixelY), int2(width, height) - 1);
            float2 child = depthPyramid.Load(int3(sourcePixel, 0));
            bounds.x = min(bounds.x, child.x);
            bounds.y = max(bounds.y, child.y);
        }
    }
    return bounds;
}
