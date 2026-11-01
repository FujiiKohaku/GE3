#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float2 centered = input.position.xy - 0.5f;
    float radius = length(centered);
    float angle = sin(
        gFrame.elapsedTime * gEffect.speed +
        radius * gEffect.frequency * 6.28318530718f +
        gEffect.phase) * gEffect.amplitude;
    float sine = sin(angle);
    float cosine = cos(angle);
    float2 twisted = float2(
        centered.x * cosine - centered.y * sine,
        centered.x * sine + centered.y * cosine);
    float4 position = input.position;
    position.xy = twisted + 0.5f;
    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
