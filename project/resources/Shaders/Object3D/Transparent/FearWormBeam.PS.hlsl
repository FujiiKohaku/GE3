#define main ShadeFearWormBeam
#include "../FearWormBeam/Render.PS.hlsl"
#undef main
#include "ObjectFog.hlsli"

float4 main(VertexShaderOutput input) : SV_Target0
{
    return ApplyObjectFog(ShadeFearWormBeam(input).color, input.worldPosition);
}
