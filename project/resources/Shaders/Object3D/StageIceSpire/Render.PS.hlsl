#include "../StageIceCommon.PS.hlsli"

StageIcePixelOutput main(VertexShaderOutput input)
{
    float4 textureColor = SampleStageIceTexture(input);
    return FinishStageIce(input,
        ShadeStageIceStructure(input, textureColor.rgb,
            float3(0.64f, 0.89f, 1.0f), 0.09f), textureColor.a);
}
