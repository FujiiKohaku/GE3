#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;

    float2 safeSpriteSize = max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float wavePhase = input.texcoord.x * gEffect.frequency * 6.28318530718f;
    wavePhase += gFrame.elapsedTime * gEffect.speed;

    float wave = sin(wavePhase);
    float edgeWeight = sin(input.texcoord.x * 3.14159265359f);
    float2 offsetPixels = gEffect.direction * gEffect.amplitude * wave * edgeWeight;

    float4 position = input.position;
    position.xy += offsetPixels / safeSpriteSize;

    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
