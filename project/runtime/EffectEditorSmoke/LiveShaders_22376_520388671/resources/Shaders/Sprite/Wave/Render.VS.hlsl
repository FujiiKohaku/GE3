#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float2 safeSpriteSize = max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float wavePhase =
        (input.texcoord.x * gEffect.frequency + gEffect.phase) * 6.28318530718f;
    wavePhase += gFrame.elapsedTime * gEffect.speed;
    float wave = sin(wavePhase);

    float2 offsetPixels =
        gEffect.direction * gEffect.amplitude * gEffect.strength * wave;
    offsetPixels *= input.texcoord.x;
    float4 position = input.position;
    position.xy += offsetPixels / safeSpriteSize;

    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
