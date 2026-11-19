#define main ShadeAssaultBullet
#include "../AssaultBullet/Render.PS.hlsl"
#undef main

float4 main(VertexShaderOutput input) : SV_Target0
{
    return ShadeAssaultBullet(input).color;
}
