#include "../Object3d.hlsli"

struct ShieldVertexOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
    float3 localPosition : TEXCOORD0;
};
