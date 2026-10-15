#define main ShadePlayerBullet
#include "../PlayerBullet/Render.PS.hlsl"
#undef main

// Submitted after distance and volumetric fog; player shots keep their original color.
float4 main(VertexShaderOutput input) : SV_Target0
{
    return ShadePlayerBullet(input).color;
}
