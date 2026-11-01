#include "../StageIceCommon.PS.hlsli"

StageIcePixelOutput main(VertexShaderOutput input)
{
    float4 textureColor = SampleStageIceTexture(input);
    return FinishStageIce(input,
        ShadeStageIceStructure(input, textureColor.rgb,
            float3(0.68f, 0.85f, 0.98f), 0.06f), textureColor.a);
}
