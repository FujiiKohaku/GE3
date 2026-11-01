#include "../StageIceCommon.PS.hlsli"

StageIcePixelOutput main(VertexShaderOutput input)
{
    float4 textureColor = SampleStageIceTexture(input);
    return FinishStageIce(input,
        ShadeStageIceStructure(input, textureColor.rgb,
            float3(0.72f, 0.86f, 0.94f), 0.025f), textureColor.a);
}
