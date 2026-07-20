struct VertexInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct TextTransform
{
    float4x4 WVP;
};

ConstantBuffer<TextTransform> gTransform : register(b0);

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.position = mul(input.position, gTransform.WVP);
    output.texcoord = input.texcoord;
    return output;
}
