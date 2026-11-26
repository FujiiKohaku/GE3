cbuffer TrailCamera : register(b0)
{
    row_major float4x4 viewProjection;
};

struct TrailVertex
{
    float3 position : POSITION;
    float side : TEXCOORD0;
    float4 color : COLOR0;
};

struct TrailFragment
{
    float4 position : SV_Position;
    float side : TEXCOORD0;
    float4 color : COLOR0;
};

TrailFragment main(TrailVertex input)
{
    TrailFragment output;
    output.position = mul(float4(input.position, 1.0f), viewProjection);
    output.side = input.side;
    output.color = input.color;
    return output;
}
