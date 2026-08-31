struct WaterPillarConstants
{
    float4x4 viewProjection;
    float4 cameraPositionAndTime;
    float4 color;
    float4 pillarPositionAndRadius;
    float4 heightAndShape;
};

ConstantBuffer<WaterPillarConstants> gPillar : register(b0);

struct WaterPillarVertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0;
    float2 texcoord : TEXCOORD0;
    float kind : TEXCOORD1;
};
