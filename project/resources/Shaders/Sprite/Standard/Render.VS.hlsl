#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
