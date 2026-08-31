#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float2 safeSize = max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float2 centered = input.texcoord - 0.5f;
    float radius = length(centered);
    float2 radialDirection = centered / max(radius, 0.001f);
    float ripple = sin(
        radius * gEffect.frequency * 6.28318530718f -
        gFrame.elapsedTime * gEffect.speed +
        gEffect.phase);
    float fade = saturate(1.0f - radius * 1.4f);
    float4 position = input.position;
    position.xy += radialDirection * ripple * fade * gEffect.amplitude / safeSize;
    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
