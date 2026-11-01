#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float pulse = 1.0f + sin(gFrame.elapsedTime * gEffect.speed + gEffect.phase) * gEffect.amplitude;
    float4 position = input.position;
    position.xy = (position.xy - 0.5f) * pulse + 0.5f;
    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
