#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float2 safeSize = max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float time = gFrame.elapsedTime * gEffect.speed + gEffect.phase;
    float2 shake = float2(sin(time * 2.13f), cos(time * 1.73f));
    float4 position = input.position;
    position.xy += shake * gEffect.direction * gEffect.amplitude / safeSize;
    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
