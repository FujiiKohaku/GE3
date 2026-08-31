#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float shear = sin(gFrame.elapsedTime * gEffect.speed + gEffect.phase) * gEffect.amplitude;
    float4 position = input.position;
    position.x += (input.position.y - 0.5f) * shear;
    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
