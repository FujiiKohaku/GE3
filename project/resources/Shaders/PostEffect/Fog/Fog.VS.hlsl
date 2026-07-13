struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(uint vertexID : SV_VertexID)
{
    VertexShaderOutput output;

    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f),
    };

    float2 texcoords[3] = {
        float2(0.0f, 1.0f),
        float2(0.0f, -1.0f),
        float2(2.0f, 1.0f),
    };

    output.position = float4(positions[vertexID], 0.0f, 1.0f);
    output.texcoord = texcoords[vertexID];

    return output;
}
