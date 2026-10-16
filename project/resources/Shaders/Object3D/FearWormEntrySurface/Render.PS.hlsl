#include "Surface.hlsli"
struct EntrySurfaceOutput {
    float4 color : SV_Target0;
    float4 encodedNormal : SV_Target1;
    float4 indirectColor : SV_Target2;
    float4 surfaceMaterial : SV_Target3;
};
EntrySurfaceOutput main(VertexShaderOutput input)
{
    EntrySurfaceOutput output = (EntrySurfaceOutput)0;
    output.color = ShadeEntrySurface(input);
    return output;
}
