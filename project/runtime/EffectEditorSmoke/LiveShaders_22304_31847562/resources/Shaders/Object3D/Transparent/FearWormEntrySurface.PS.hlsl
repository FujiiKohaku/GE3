#include "../FearWormEntrySurface/Surface.hlsli"
#include "ObjectFog.hlsli"
float4 main(VertexShaderOutput input) : SV_Target0
{
    return ApplyObjectFog(ShadeEntrySurface(input), input.worldPosition);
}
