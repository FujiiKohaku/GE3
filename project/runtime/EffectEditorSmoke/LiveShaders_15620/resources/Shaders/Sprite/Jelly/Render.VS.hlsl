#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float2 safeSize = max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float time = gFrame.elapsedTime * gEffect.speed + gEffect.phase;
    float waveX = sin(input.texcoord.y * gEffect.frequency * 6.28318530718f + time);
    float waveY = cos(input.texcoord.x * gEffect.frequency * 6.28318530718f + time * 0.83f);
    float2 offsetPixels = float2(waveX, waveY) * gEffect.amplitude;
    float4 position = input.position;
    position.xy += offsetPixels / safeSize;
    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
