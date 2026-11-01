#include "Blur.hlsli"
float4 main(PixelInput input) : SV_TARGET
{
    return BlurReflection(input, float2(0, 1));
}
