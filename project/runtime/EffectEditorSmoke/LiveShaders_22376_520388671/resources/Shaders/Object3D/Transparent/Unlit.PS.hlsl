#define main ShadeUnlit
#include "../Unlit/Render.PS.hlsl"
#undef main
#include "ObjectFog.hlsli"

float4 main(VertexShaderOutput input) : SV_Target0
{
    return ApplyObjectFog(ShadeUnlit(input).color, input.worldPosition);
}
