#define main ShadePlayerShield
#include "../PlayerShield/Render.PS.hlsl"
#undef main
#include "ObjectFog.hlsli"

float4 main(ShieldVertexOutput input, bool isFrontFace : SV_IsFrontFace) : SV_Target0
{
    // 破壊直後にシールド表面へ走るヒビにも透過フォグを適用する。
    return ApplyObjectFog(ShadePlayerShield(input, isFrontFace), input.worldPosition);
}
