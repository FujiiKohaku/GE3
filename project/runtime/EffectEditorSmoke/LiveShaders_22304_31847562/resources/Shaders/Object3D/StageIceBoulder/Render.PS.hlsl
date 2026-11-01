#include "../StageIceCommon.PS.hlsli"

StageIcePixelOutput main(VertexShaderOutput input)
{
    float4 textureColor = SampleStageIceTexture(input);
    return FinishStageIce(input,
        ShadeStageIceStructure(input, textureColor.rgb,
            float3(0.62f, 0.79f, 0.90f), 0.035f), textureColor.a);
}
