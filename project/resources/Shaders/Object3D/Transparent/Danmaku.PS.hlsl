#define main ShadeDanmaku
#include "../DanmakuBullet/Render.PS.hlsl"
#undef main
#include "ObjectFog.hlsli"

float4 main(VertexShaderOutput input) : SV_Target0
{
    return ApplyObjectFog(ShadeDanmaku(input).color, input.worldPosition);
}
