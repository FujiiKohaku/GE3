#include "../StageIceCommon.PS.hlsli"

StageIcePixelOutput main(VertexShaderOutput input)
{
    float4 textureColor = SampleStageIceTexture(input);
    return FinishStageIce(input,
        ShadeStageIceStructure(input, textureColor.rgb,
            float3(0.78f, 0.94f, 1.0f), 0.11f), textureColor.a);
}
