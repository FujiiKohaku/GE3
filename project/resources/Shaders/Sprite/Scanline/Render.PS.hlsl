#include "../Common/SpritePixelCommon.hlsli"

struct PixelShaderOutput { float4 color : SV_Target0; };

PixelShaderOutput main(SpriteVertexOutput input)
{
    PixelShaderOutput output;
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 color = gTexture.Sample(gSampler, uv) * gMaterial.color;
    float scan = 0.5f + 0.5f * sin(
        input.texcoord.y * gEffect.frequency + gFrame.elapsedTime * gEffect.speed);
    color.rgb *= 1.0f - scan * gEffect.strength;
    output.color = color;
    return output;
}
