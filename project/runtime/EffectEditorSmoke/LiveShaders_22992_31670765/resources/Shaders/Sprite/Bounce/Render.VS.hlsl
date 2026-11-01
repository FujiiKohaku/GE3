#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float2 safeSize = max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float bounce = abs(sin(gFrame.elapsedTime * gEffect.speed + gEffect.phase));
    float4 position = input.position;
    position.xy -= gEffect.direction * gEffect.amplitude * bounce / safeSize;
    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
