// Legacy Ice is also used by the skinning pipeline, which has no shadow inputs.
#define KOHAKU_NO_SHADOWS
#include "../StageIceCommon.PS.hlsli"

StageIcePixelOutput main(VertexShaderOutput input)
{
    float4 textureColor = SampleStageIceTexture(input);
    float3 iceTexture = lerp(float3(0.80f, 0.88f, 0.95f), textureColor.rgb, 0.52f);
    return FinishStageIce(input,
        ShadeStageIceStructure(input, iceTexture,
            float3(0.64f, 0.85f, 0.98f), 0.065f), textureColor.a);
}
