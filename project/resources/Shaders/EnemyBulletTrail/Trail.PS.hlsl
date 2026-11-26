struct TrailFragment
{
    float4 position : SV_Position;
    float side : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(TrailFragment input) : SV_Target0
{
    const float kEdgeFeatherScale = 8.0f;
    float edgeCoverage = saturate((1.0f - abs(input.side)) * kEdgeFeatherScale);
    return float4(input.color.rgb, input.color.a * edgeCoverage);
}
