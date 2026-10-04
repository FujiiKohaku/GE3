#include "Blur.hlsli"
float4 main(PixelInput input) : SV_TARGET { return BlurIndirect(input, int2(0, 1)); }
