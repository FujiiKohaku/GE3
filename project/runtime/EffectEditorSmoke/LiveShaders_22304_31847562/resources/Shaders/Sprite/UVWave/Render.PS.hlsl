#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput { float4 color : SV_Target0; };

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 uv = input.texcoord;
    float wave = sin(uv.y * gEffect.frequency + gFrame.elapsedTime * gEffect.speed + gEffect.phase);
    uv += gEffect.direction * wave * gEffect.amplitude;
    uv = mul(float4(uv, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    output.color = gTexture.Sample(gSampler, uv) * gMaterial.color;
    return output;
}
