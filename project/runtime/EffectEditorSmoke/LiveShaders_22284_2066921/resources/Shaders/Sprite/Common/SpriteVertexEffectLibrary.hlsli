#include "SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float4 position = input.position;
    float2 centered = input.position.xy - 0.5f;
    float2 safeSize = max(gEffect.spriteSize, float2(1.0f, 1.0f));
    float time = gFrame.elapsedTime * gEffect.speed + gEffect.phase;

#if SPRITE_VERTEX_EFFECT == 1
    float angle = sin(time) * gEffect.amplitude;
    float sine = sin(angle);
    float cosine = cos(angle);
    float2 pivotPosition = input.position.xy - float2(0.5f, 0.0f);
    position.xy = float2(
        pivotPosition.x * cosine - pivotPosition.y * sine,
        pivotPosition.x * sine + pivotPosition.y * cosine) + float2(0.5f, 0.0f);
#elif SPRITE_VERTEX_EFFECT == 2
    float squash = sin(time) * gEffect.amplitude;
    centered.x *= 1.0f + squash;
    centered.y *= 1.0f - squash;
    position.xy = centered + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 3
    float fold = sin(input.texcoord.x * gEffect.frequency * 6.28318530718f + time);
    position.x += fold * gEffect.amplitude;
    position.y += abs(fold) * gEffect.amplitude * 0.35f;
#elif SPRITE_VERTEX_EFFECT == 4
    float radius = length(centered);
    float spiralAngle = radius * gEffect.frequency + time;
    float spiralSine = sin(spiralAngle * gEffect.amplitude);
    float spiralCosine = cos(spiralAngle * gEffect.amplitude);
    position.xy = float2(
        centered.x * spiralCosine - centered.y * spiralSine,
        centered.x * spiralSine + centered.y * spiralCosine) + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 5
    float bulgeRadius = length(centered) * 2.0f;
    float bulge = (1.0f - saturate(bulgeRadius)) * sin(time) * gEffect.amplitude;
    position.xy = centered * (1.0f + bulge) + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 6
    float pinchRadius = length(centered) * 2.0f;
    float pinch = (1.0f - saturate(pinchRadius)) * (0.5f + 0.5f * sin(time));
    position.xy = centered * (1.0f - pinch * gEffect.amplitude) + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 7
    float curl = sin(input.texcoord.x * 3.14159265359f + time);
    float curlEnvelope = input.texcoord.x * input.texcoord.x;
    position.y += curl * curlEnvelope * gEffect.amplitude / safeSize.y;
    position.x -= abs(curl) * curlEnvelope * gEffect.amplitude * 0.4f / safeSize.x;
#elif SPRITE_VERTEX_EFFECT == 8
    float wind = sin(
        input.texcoord.x * gEffect.frequency * 6.28318530718f +
        input.texcoord.y * 2.0f + time);
    position.xy += gEffect.direction * wind * input.texcoord.x * gEffect.amplitude / safeSize;
#elif SPRITE_VERTEX_EFFECT == 9
    float2 orbit = float2(cos(time), sin(time));
    position.xy += orbit * gEffect.amplitude / safeSize;
#elif SPRITE_VERTEX_EFFECT == 10
    float flip = cos(time);
    centered.x *= flip;
    centered.y *= 0.88f + abs(flip) * 0.12f;
    position.xy = centered + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 11
    float elasticWave = sin(time * gEffect.frequency) * cos(time * 0.37f);
    centered.x *= 1.0f + elasticWave * gEffect.amplitude;
    centered.y *= 1.0f - elasticWave * gEffect.amplitude * 0.65f;
    position.xy = centered + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 12
    float sineScale = sin(time) * gEffect.amplitude;
    centered.x *= 1.0f + sineScale;
    centered.y *= 1.0f - sineScale;
    position.xy = centered + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 13
    float cornerWeight = input.texcoord.x * (1.0f - input.texcoord.y);
    float cornerMove = sin(time) * gEffect.amplitude * cornerWeight;
    position.xy += gEffect.direction * cornerMove / safeSize;
#elif SPRITE_VERTEX_EFFECT == 14
    float slant = sin(time) * gEffect.amplitude;
    position.x += (input.texcoord.y - 0.5f) * slant;
    position.y += (input.texcoord.x - 0.5f) * slant * 0.25f;
#elif SPRITE_VERTEX_EFFECT == 15
    float breathing = sin(time) * gEffect.amplitude;
    centered *= 1.0f + breathing;
    centered.y += breathing * 0.18f;
    position.xy = centered + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 16
    float shockwaveRadius = length(centered) * 2.0f;
    float2 shockwaveDirection = centered / max(length(centered), 0.001f);
    float shockwave = sin(shockwaveRadius * gEffect.frequency * 6.28318530718f - time);
    float shockwaveEnvelope = saturate(1.0f - shockwaveRadius);
    position.xy += shockwaveDirection * shockwave * shockwaveEnvelope * gEffect.amplitude / safeSize;
#elif SPRITE_VERTEX_EFFECT == 17
    float diamondDistance = abs(centered.x) + abs(centered.y);
    float diamondWave = sin(diamondDistance * gEffect.frequency * 6.28318530718f + time);
    float diamondScale = 1.0f + diamondWave * gEffect.amplitude * saturate(1.0f - diamondDistance);
    position.xy = centered * diamondScale + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 18
    float fanFold = sin(input.texcoord.x * gEffect.frequency * 3.14159265359f + time);
    float fanEnvelope = input.texcoord.y;
    position.x += fanFold * fanEnvelope * gEffect.amplitude;
    position.y += abs(fanFold) * fanEnvelope * gEffect.amplitude * 0.35f;
#elif SPRITE_VERTEX_EFFECT == 19
    float pendulumAngle = sin(time) * gEffect.amplitude;
    float pendulumSine = sin(pendulumAngle);
    float pendulumCosine = cos(pendulumAngle);
    float2 pendulumPivot = input.position.xy - float2(0.5f, 0.0f);
    position.xy = float2(
        pendulumPivot.x * pendulumCosine - pendulumPivot.y * pendulumSine,
        pendulumPivot.x * pendulumSine + pendulumPivot.y * pendulumCosine) + float2(0.5f, 0.0f);
    position.y += abs(sin(time)) * 0.035f;
#elif SPRITE_VERTEX_EFFECT == 20
    float popWave = sin(time) + sin(time * 3.0f) * 0.25f;
    float popScale = 1.0f + popWave * gEffect.amplitude;
    position.xy = centered * popScale + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 21
    float ribbonPhase = input.texcoord.x * gEffect.frequency * 6.28318530718f + time;
    float ribbonWave = sin(ribbonPhase);
    position.y += ribbonWave * gEffect.amplitude / safeSize.y;
    position.x += cos(ribbonPhase) * gEffect.amplitude * 0.25f / safeSize.x;
#elif SPRITE_VERTEX_EFFECT == 22
    float verticalStretch = sin(time) * gEffect.amplitude;
    centered.y *= 1.0f + verticalStretch;
    centered.x *= 1.0f - verticalStretch * 0.35f;
    position.xy = centered + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 23
    float fishEyeRadius = length(centered) * 2.0f;
    float fishEyeEnvelope = 1.0f - saturate(fishEyeRadius);
    float fishEyeScale = 1.0f + fishEyeEnvelope * fishEyeEnvelope * gEffect.amplitude * (0.65f + 0.35f * sin(time));
    position.xy = centered * fishEyeScale + 0.5f;
#elif SPRITE_VERTEX_EFFECT == 24
    float cornerWavePhase = (input.texcoord.x + input.texcoord.y) * gEffect.frequency * 3.14159265359f + time;
    float cornerWaveWeight = input.texcoord.x * input.texcoord.y;
    position.xy += gEffect.direction * sin(cornerWavePhase) * cornerWaveWeight * gEffect.amplitude / safeSize;
#elif SPRITE_VERTEX_EFFECT == 25
    float waveZoomRadius = length(centered) * 2.0f;
    float waveZoom = sin(waveZoomRadius * gEffect.frequency * 6.28318530718f - time) * gEffect.amplitude;
    position.xy = centered * (1.0f + waveZoom) + 0.5f;
#endif

    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
